// Round fourteen's group R1F - the 49 functions the cut lists for R1F
// (analysis/round14_cut.tsv), 0x523ED0..0x52899B, each read with capstone to
// its last instruction (docs/rest_1f.md section 1): party sets 16, 17 and 18's
// field actions, a raised sprite's cell ahead and its helpers, and the
// leader's state 9's stage 0.
//
// Every call out goes through the scenario harness (SH_CALL, or
// scenario_harness::Call on a function pointer), so the start-up fuzz can
// stand recorders in for ours as for the originals' copies; the .data tables
// the dispatchers jump through are read in place (the fuzz swaps their cells
// for recorders). No divergence: each is a faithful replacement. Where an
// original indexes a table or a record array by a byte it does not bound -
// the dispatchers' state bytes and form words, an object index from
// Sprite_ObjectAt, an effect index - ours aborts with a message past the
// table's or the array's end (docs/rest_1f.md section 7).
#include "game/rest_1f.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();
using Pickup = unsigned char (__cdecl*)(unsigned, unsigned);

constexpr U kSteps = 0x6697B0;         // Field_DirectionSteps: 8 rows of two longs (x, z)
constexpr U kCellOffsets = 0x66971C;   // two signed bytes a direction: the cell ahead
constexpr U kCells = 0x903850;         // DamageScratch: cell 0 the sloped flag, 1..7 the kinds, 8..0xB the classes
constexpr U kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 0x1E;   // Sprite_Objects; 0x1E.. Sprite_ObjectsExtra's four
constexpr unsigned kEffectCount = 20;
constexpr U kEffectsAt = 0x7E11E0;     // Effect_Objects (its name is a macro here)
constexpr U kObjectsAt = 0x7DEE80;     // Sprite_Objects
constexpr U kExtraAt = 0x802000;       // Sprite_ObjectsExtra
constexpr U kEffectStride = 0x80;
constexpr U kPanelPose = 0x6BC716;     // the leader panel's cells (docs/effect_1e.md section 1)
constexpr U kPanelCounter = 0x6BC709;
constexpr U kPanelBlink = 0x939A28;    // FieldPanel_DrawBlink's switch

unsigned char* Sc() { return Sprite_Current; }
unsigned char& Cell(unsigned i) { return At(kCells)[i & 0xFF]; }
// A Field_DirectionSteps row, read in place with the direction unmasked (the
// originals' `shl eax, 3` on the zero-extended byte): a byte above 7 reads the
// .data after the table, as they do.
U StepX(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8))); }
U StepZ(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8 + 4))); }
// The pose offset of a direction: (d - 1) / 2 as the originals compute it
// (`dec eax; cdq; sub eax, edx; sar eax, 1`), so direction 0 gives 0.
unsigned char HalfTurn(unsigned char d) { return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2); }
// `cdq; sub eax, edx; sar eax, 1`: a signed halving that rounds toward zero.
long Half(U v) {
    const auto s = static_cast<std::int32_t>(v);
    return static_cast<long>(static_cast<std::int32_t>(static_cast<U>(s) + (s < 0 ? 1u : 0u)) >> 1);
}
std::uint16_t W16(unsigned v) { return static_cast<std::uint16_t>(v); }

// A dispatcher: the table's entry by `index`, called as the original jumps
// to it - a Fatal past the table's own entries, where the original jumps
// through the dword after (docs/rest_1f.md section 7).
void Dispatch(const char* who, const char* by, const unsigned long* table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1f.md section 7)",
                    who, by, index, count, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(table)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}
void ByState(const char* who, const unsigned long* table, unsigned count) {
    Dispatch(who, "Sprite_Current +2", table, count, Sc()[2]);
}
void ByStep(const char* who, const unsigned long* table, unsigned count) {
    Dispatch(who, "Sprite_Current +3", table, count, Sc()[3]);
}
void ByForm(const char* who, const unsigned long* table, unsigned count) {
    Dispatch(who, "Sprite_Current's word +0x2C", table, count, Word(Sc() + 0x2C));
}

// An Effect_Objects record by an index byte the original uses unchecked
// (`shl reg, 7`): a Fatal past the 20 records.
unsigned char* EffectAt(unsigned index, const char* who) {
    if (index >= kEffectCount)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original writes 0x%X "
                    "(docs/rest_1f.md section 7)",
                    who, index, static_cast<unsigned>(kEffectsAt + index * kEffectStride));
    return Effect_Objects + index * kEffectStride;
}

// The object Sprite_ObjectAt found gets bit 0 of its +0x80: 0..0x1D a
// Sprite_Objects record, 0x1E.. one of Sprite_ObjectsExtra's four (`cmp al,
// 0x1E; movsx eax, al; jge`, a signed byte). Sprite_ObjectAt answers 0..0x21
// or 0xFF; any other byte - a negative one indexes before Sprite_Objects -
// aborts here.
void MarkObject(unsigned char found, const char* who) {
    const int i = static_cast<signed char>(found);
    if (i >= 0 && i < static_cast<int>(kObjectCount))
        Sprite_Objects[static_cast<unsigned>(i) * kObjectStride + 0x80] |= 1;
    else if (i >= static_cast<int>(kObjectCount) && i < static_cast<int>(kObjectCount) + 4)
        Sprite_ObjectsExtra[static_cast<unsigned>(i - static_cast<int>(kObjectCount)) * kObjectStride + 0x80] |= 1;
    else
        bof3::Fatal("%s: Sprite_ObjectAt's answer 0x%02X is past Sprite_Objects' 30 and Sprite_ObjectsExtra's 4; the "
                    "original sets a bit at 0x%X (docs/rest_1f.md section 7)",
                    who, found,
                    static_cast<unsigned>(i < static_cast<int>(kObjectCount)
                                              ? kObjectsAt + 0x80u + static_cast<U>(i * static_cast<int>(kObjectStride))
                                              : kExtraAt + 0x80u +
                                                    static_cast<U>((i - static_cast<int>(kObjectCount)) * static_cast<int>(kObjectStride))));
}

// The 16 bytes of an item's name copied to Text_Records, as four dwords.
void CopyName(const unsigned char* name) {
    for (unsigned k = 0; k < 16; k += 4) SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
}

// --- the shapes three sets share (docs/rest_1f.md section 1.2) -------------------------------

// The side probe the Begin states make inline (PartyAction_SideProbes' probe,
// not a call to it): the point one Field_DirectionSteps row `d` (3 or 5, the
// rows read by address) from Sprite_Current; the slope there steep (the
// scratch flag set and its low word above 0x40, signed) and the ground there
// above Sprite_Current's height word (s16 against s16): +0x2B = 0.
void SideProbe(unsigned d) {
    const unsigned char* const s = Sc();
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (At(bof3::addr::DamageScratch)[0] == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    if (static_cast<short>(Word(Sc() + 0x3E)) < static_cast<short>(ground)) Sc()[0x2B] = 0;
}

// 0x523FF0 / 0x524490 / 0x524E90 (byte-identical): a form's state 0.
void BeginShape() {
    unsigned char* s = Sc();
    const unsigned char d0 = s[8];
    if ((d0 & 1) == 0) {
        s[8] = static_cast<unsigned char>((d0 - 1) & 7);
        if (SH_CALL(PartyAction_TargetAhead)() == 0) {
            Sc()[8] = static_cast<unsigned char>((Sc()[8] + 2) & 7);
            if (SH_CALL(PartyAction_TargetAhead)() == 0) Sc()[8] = static_cast<unsigned char>((Sc()[8] - 2) & 7);
        }
        s = Sc();
    }
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(Sc() + 0x3E)));
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), Sc()[8]);
    if (At(bof3::addr::DamageScratch)[0] != 0 && rise > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x46));
        ++Sc()[2];
    } else {
        Sc()[0x2B] = 1;
        SideProbe(3);
        SideProbe(5);
        SH_CALL(Sound_PlayEffect)(W16(Word(Sc() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
        Sc()[0xA] = 5;
    }
    Sc()[0xB] = 0;
    ++Sc()[2];
}

// 0x5241D0 / 0x524670 / 0x525070 (byte-identical but for the pickup they
// call): a form's state 1, PartyAction5_Form0Resolve's code with its own
// pickup.
void ResolveShape(Pickup pickup, const char* who) {
    --Sc()[0xA];
    if (Sc()[0xA] == 0) {
        const unsigned char* const s = Sc();
        const unsigned d = s[8];
        const U x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
        const U z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (found != 0xFF) MarkObject(found, who);
        const unsigned cx = x >> 16, cz = z >> 16;
        if (scenario_harness::Call(pickup)(cx, cz) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = scenario_harness::Call(pickup)(cx + 1, cz) != 0;
            if (!done && (z & 0xFFFF) != 0) scenario_harness::Call(pickup)(cx, cz + 1);
        }
        ++Sc()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x5242B0 / 0x524750 / 0x525150 (byte-identical): Field_CellPickup's code
// with one constant changed - the found zenny times 20, not 10.
unsigned char PickupShape(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 20);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sc()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        CopyName(name);
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

// 0x523ED0 / 0x524D60 (byte-identical): PartyAction_SideProbes, the pose
// (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1.
void ProbeStartShape() {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
    Sc()[0xA] = 0xB;
    Sc()[3] = 1;
}

}  // namespace

// ===========================================================================
// The dispatchers (docs/rest_1f.md section 3: each table's entries)
// ===========================================================================

// original 0x5243D0 (0x13 bytes): Field_FormActions[16] - PartyAction16_FormActionForms by the word +0x2C.
extern "C" void __cdecl PartyAction16_FormAction(void) {
    ByForm("PartyAction16_FormAction (0x5243D0)", PartyAction16_FormActionForms, PartyAction16_FormActionForms_count);
}
// original 0x5243F0 (0x13 bytes): Field_ActionBySet[16] - PartyAction16_Forms by the word +0x2C.
extern "C" void __cdecl PartyAction16_ByForm(void) {
    ByForm("PartyAction16_ByForm (0x5243F0)", PartyAction16_Forms, PartyAction16_Forms_count);
}
// original 0x523FB0 (0x12 bytes): PartyAction16_FormActionForms[2] - PartyAction16_FormAction2States by +2.
extern "C" void __cdecl PartyAction16_FormAction2(void) {
    ByState("PartyAction16_FormAction2 (0x523FB0)", PartyAction16_FormAction2States, PartyAction16_FormAction2States_count);
}
// original 0x523FD0 (0x12 bytes): PartyAction16_Forms[2] - PartyAction16_Form2States by +2.
extern "C" void __cdecl PartyAction16_Form2(void) {
    ByState("PartyAction16_Form2 (0x523FD0)", PartyAction16_Form2States, PartyAction16_Form2States_count);
}
// original 0x524930 (0x13 bytes): Field_FormActions[17] - PartyAction17_FormActionForms by the word +0x2C.
extern "C" void __cdecl PartyAction17_FormAction(void) {
    ByForm("PartyAction17_FormAction (0x524930)", PartyAction17_FormActionForms, PartyAction17_FormActionForms_count);
}
// original 0x524950 (0x13 bytes): Field_ActionBySet[17] - PartyAction17_Forms by the word +0x2C.
extern "C" void __cdecl PartyAction17_ByForm(void) {
    ByForm("PartyAction17_ByForm (0x524950)", PartyAction17_Forms, PartyAction17_Forms_count);
}
// original 0x524410 (0x12 bytes): PartyAction17_FormActionForms[0] - PartyAction17_FormAction0States by +2.
extern "C" void __cdecl PartyAction17_FormAction0(void) {
    ByState("PartyAction17_FormAction0 (0x524410)", PartyAction17_FormAction0States, PartyAction17_FormAction0States_count);
}
// original 0x524450 (0x12 bytes): PartyAction17_FormActionForms[1] - PartyAction17_FormAction1States by +2.
extern "C" void __cdecl PartyAction17_FormAction1(void) {
    ByState("PartyAction17_FormAction1 (0x524450)", PartyAction17_FormAction1States, PartyAction17_FormAction1States_count);
}
// original 0x5248F0 (0x12 bytes): PartyAction17_FormActionForms[2] - PartyAction17_FormAction2States by +2.
extern "C" void __cdecl PartyAction17_FormAction2(void) {
    ByState("PartyAction17_FormAction2 (0x5248F0)", PartyAction17_FormAction2States, PartyAction17_FormAction2States_count);
}
// original 0x524430 (0x12 bytes): PartyAction17_Forms[0] - PartyAction17_Form0States by +2.
extern "C" void __cdecl PartyAction17_Form0(void) {
    ByState("PartyAction17_Form0 (0x524430)", PartyAction17_Form0States, PartyAction17_Form0States_count);
}
// original 0x524470 (0x12 bytes): PartyAction17_Forms[1] - PartyAction17_Form1States by +2.
extern "C" void __cdecl PartyAction17_Form1(void) {
    ByState("PartyAction17_Form1 (0x524470)", PartyAction17_Form1States, PartyAction17_Form1States_count);
}
// original 0x524910 (0x12 bytes): PartyAction17_Forms[2] - PartyAction17_Form2States by +2.
extern "C" void __cdecl PartyAction17_Form2(void) {
    ByState("PartyAction17_Form2 (0x524910)", PartyAction17_Form2States, PartyAction17_Form2States_count);
}
// original 0x525330 (0x13 bytes): Field_FormActions[18] - PartyAction18_FormActionForms by the word +0x2C.
extern "C" void __cdecl PartyAction18_FormAction(void) {
    ByForm("PartyAction18_FormAction (0x525330)", PartyAction18_FormActionForms, PartyAction18_FormActionForms_count);
}
// original 0x525350 (0x13 bytes): Field_ActionBySet[18] - PartyAction18_Forms by the word +0x2C.
extern "C" void __cdecl PartyAction18_ByForm(void) {
    ByForm("PartyAction18_ByForm (0x525350)", PartyAction18_Forms, PartyAction18_Forms_count);
}
// original 0x524970 (0x12 bytes): PartyAction18_FormActionForms[0] - PartyAction18_FormAction0States by +2.
extern "C" void __cdecl PartyAction18_FormAction0(void) {
    ByState("PartyAction18_FormAction0 (0x524970)", PartyAction18_FormAction0States, PartyAction18_FormAction0States_count);
}
// original 0x524E50 (0x12 bytes): PartyAction18_FormActionForms[1] - PartyAction18_FormAction1States by +2.
extern "C" void __cdecl PartyAction18_FormAction1(void) {
    ByState("PartyAction18_FormAction1 (0x524E50)", PartyAction18_FormAction1States, PartyAction18_FormAction1States_count);
}
// original 0x525270 (0x12 bytes): PartyAction18_FormActionForms[2] - PartyAction18_FormAction2States by +2.
extern "C" void __cdecl PartyAction18_FormAction2(void) {
    ByState("PartyAction18_FormAction2 (0x525270)", PartyAction18_FormAction2States, PartyAction18_FormAction2States_count);
}
// original 0x524990 (0x12 bytes): PartyAction18_Forms[0] - PartyAction18_Form0Subs by +2.
extern "C" void __cdecl PartyAction18_Form0(void) {
    ByState("PartyAction18_Form0 (0x524990)", PartyAction18_Form0Subs, PartyAction18_Form0Subs_count);
}
// original 0x5249B0 (0x12 bytes): PartyAction18_Form0Subs[0] - PartyAction18_Form0Sub0Steps by +3.
extern "C" void __cdecl PartyAction18_Form0Sub0(void) {
    ByStep("PartyAction18_Form0Sub0 (0x5249B0)", PartyAction18_Form0Sub0Steps, PartyAction18_Form0Sub0Steps_count);
}
// original 0x524D40 (0x12 bytes): PartyAction18_Form0Subs[1] - PartyAction18_Form0Sub1Steps by +3.
extern "C" void __cdecl PartyAction18_Form0Sub1(void) {
    ByStep("PartyAction18_Form0Sub1 (0x524D40)", PartyAction18_Form0Sub1Steps, PartyAction18_Form0Sub1Steps_count);
}
// original 0x524E70 (0x12 bytes): PartyAction18_Forms[1] - PartyAction18_Form1States by +2.
extern "C" void __cdecl PartyAction18_Form1(void) {
    ByState("PartyAction18_Form1 (0x524E70)", PartyAction18_Form1States, PartyAction18_Form1States_count);
}
// original 0x525290 (0x12 bytes): PartyAction18_Forms[2] - PartyAction18_Form2States by +2.
extern "C" void __cdecl PartyAction18_Form2(void) {
    ByState("PartyAction18_Form2 (0x525290)", PartyAction18_Form2States, PartyAction18_Form2States_count);
}
// original 0x528880 (0x12 bytes): Field_LeaderStates[9] - LeaderPanel_Stages by +2.
extern "C" void __cdecl LeaderPanel_Run(void) {
    ByState("LeaderPanel_Run (0x528880)", LeaderPanel_Stages, LeaderPanel_Stages_count);
}
// original 0x5288A0 (0x12 bytes): LeaderPanel_Stages[0] - LeaderPanel_Stage0Steps by +3.
extern "C" void __cdecl LeaderPanel_S0(void) {
    ByStep("LeaderPanel_S0 (0x5288A0)", LeaderPanel_Stage0Steps, LeaderPanel_Stage0Steps_count);
}

// ===========================================================================
// The state handlers
// ===========================================================================

// original 0x523FF0 (0x1D4 bytes), PartyAction16_Form2States[0]; 0x524490
// (PartyAction17_Form1States[0]) and 0x524E90 (PartyAction18_Form1States[0])
// are the same bytes. An even direction +8 is turned one eighth back (-1, &
// 7); when PartyAction_TargetAhead finds nothing that way, two on (+2), and
// when it finds nothing there either, back (-2). Then the point one step
// ahead in +8: MapView_GroundAt there, less Sprite_Current's height word (16
// bits), is the rise; MapView_SlopeAt there in +8 (its answer unused). With
// the scratch flag set and the rise above 0x40 (s16): the pose (+8 - 1) / 2 +
// 0x46 and +2 one on. Otherwise +0x2B = 1, the side probes in directions 3
// and 5 (inline), Sound_PlayEffect(u16 +0x2C + 0x100), the pose (+8 - 1) / 2
// + 0x42 and +0xA = 5. Then +0xB = 0 and +2 one on.
extern "C" void __cdecl PartyAction16_Form2Begin(void) { BeginShape(); }
extern "C" void __cdecl PartyAction17_Form1Begin(void) { BeginShape(); }
extern "C" void __cdecl PartyAction18_Form1Begin(void) { BeginShape(); }

// original 0x5241D0 (0xDB bytes), PartyAction16_Form2States[1]; 0x524670 and
// 0x525070 the same with their own pickups. +0xA counted down; at 0: the
// point two steps ahead in +8 (unchecked); the object Sprite_ObjectAt(point,
// 0) finds gets bit 0 of its +0x80; then the pickup on the point's cell, and
// when that finds nothing, on the cell one on in x (only when the point's x
// has a fraction), then one on in z (when z has one); then +2 one on.
// Sprite_ScriptTickOnce every time. The cells are the point's high words; the
// original passes them as dwords whose upper halves are its own uninitialised
// stack, and each pickup reads their low words only.
extern "C" void __cdecl PartyAction16_Form2Resolve(void) {
    ResolveShape(&PartyAction16_CellPickup, "PartyAction16_Form2Resolve (0x5241D0)");
}
extern "C" void __cdecl PartyAction17_Form1Resolve(void) {
    ResolveShape(&PartyAction17_CellPickup, "PartyAction17_Form1Resolve (0x524670)");
}
extern "C" void __cdecl PartyAction18_Form1Resolve(void) {
    ResolveShape(&PartyAction18_CellPickup, "PartyAction18_Form1Resolve (0x525070)");
}

// original 0x5242B0 (0x11F bytes); 0x524750 and 0x525150 are the same bytes.
// What lies in the map cell (x, z) - AreaMap_ByteAt:
//   0xF2 - with an effect object free (Effect_FindFree): Effect_SpawnAtCell(0,
//          x, z); three Rand nibbles in sixteen (13, 14, 15) find zenny, 2 (5
//          on 15), twenty times that when Field_InputFlags has bit 1 or 2 and
//          a second Rand & 3 is 0: Field_GiveZenny(the amount),
//          Effect_SpawnAtCell(1, x, z); then +0xB = 1. None free: none of it.
//   0xF8 - Effect_SpawnAtCell(0, x, z); item 0x56's name (Item_NamePtr(0,
//          0x56)), 16 bytes, to Text_Records; Inventory_Add(0, 0x56, 1)
//          taken: Sound_PlayEffect(0x106), Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (AreaMap_ClearCell) and answer 1; any other cell 0.
// Field_CellPickup 0x51EBD0 is the same code with ten times, not twenty.
extern "C" unsigned char __cdecl PartyAction16_CellPickup(unsigned x, unsigned z) { return PickupShape(x, z); }
extern "C" unsigned char __cdecl PartyAction17_CellPickup(unsigned x, unsigned z) { return PickupShape(x, z); }
extern "C" unsigned char __cdecl PartyAction18_CellPickup(unsigned x, unsigned z) { return PickupShape(x, z); }

// original 0x523ED0 (0x35 bytes), and 0x524D60 the same bytes:
// PartyAction_SideProbes, the pose (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction_ProbeStart(void) { ProbeStartShape(); }
extern "C" void __cdecl PartyAction18_ProbeStart(void) { ProbeStartShape(); }

// original 0x523F10 (0x97 bytes), the step after a ProbeStart in seven state
// tables: Sprite_ScriptTickOnce; when it answers non-zero, the pose +8
// (Sprite_EnsureAnimation) and +3 = 2. Otherwise +0xA, when not 0, counted
// down; reaching 0: Sound_PlayEffect(u16 +0x2C + 0x100), then effect object
// +0xB's +8 = Sprite_Current's +8, +7 = 1 when it was 0, the object's +0xA =
// +7, Sound_PlayEffect(0x10B). The effect index +0xB is not checked by the
// original: ours aborts past the 20 records.
extern "C" void __cdecl PartyAction_EffectCountdown(void) {
    const char* const who = "PartyAction_EffectCountdown (0x523F10)";
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
        SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
        Sc()[3] = 2;
        return;
    }
    unsigned char* const s = Sc();
    if (s[0xA] == 0) return;
    --s[0xA];
    if (s[0xA] != 0) return;
    SH_CALL(Sound_PlayEffect)(W16(Word(s + 0x2C) + 0x100));
    unsigned char* const c = Sc();
    EffectAt(c[0xB], who)[8] = c[8];
    if (c[7] == 0) c[7] = 1;
    EffectAt(c[0xB], who)[0xA] = c[7];
    SH_CALL(Sound_PlayEffect)(0x10B);
}

// original 0x5252B0 (0x7B bytes), a state in seven tables: an even direction
// +8 turned one eighth back (-1, & 7); +0xB = Effect_FindFree; when it is not
// 0xFF, that effect object's +0 = 1 and kind +5 = 0x1B (the index unchecked:
// ours aborts past the 20 records). The pose +8 >> 1 (unsigned) + 0x42,
// Sound_PlayEffect(u16 +0x2C + 0x100), +2 one on.
extern "C" void __cdecl PartyAction_SpawnKind1B(void) {
    const char* const who = "PartyAction_SpawnKind1B (0x5252B0)";
    unsigned char* const s = Sc();
    if ((s[8] & 1) == 0) s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Sc()[0xB] = slot;
    unsigned char* const c = Sc();
    if (c[0xB] != 0xFF) {
        EffectAt(c[0xB], who)[0] = 1;
        EffectAt(c[0xB], who)[5] = 0x1B;
    }
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>((c[8] >> 1) + 0x42));
    SH_CALL(Sound_PlayEffect)(W16(Word(Sc() + 0x2C) + 0x100));
    ++Sc()[2];
}

// original 0x5249D0 (0x87 bytes), PartyAction18_Form0Sub0Steps[0]: an even
// direction turned one eighth back; when PartyAction_BlockedAhead finds the
// way open, two on; open there too, back. Then PartyAction_SideProbes, the
// pose (+8 - 1) / 2 + 0x42, +0xB = 0, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction18_Form0Sub0Begin(void) {
    unsigned char* const s = Sc();
    const unsigned char d0 = s[8];
    if ((d0 & 1) == 0) {
        s[8] = static_cast<unsigned char>((d0 - 1) & 7);
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

// original 0x524A60 (0x14C bytes), PartyAction18_Form0Sub0Steps[1]: +0xA,
// when not 0, counted down; reaching 0: Sound_PlayEffect(u16 +0x2C + 0x100);
// Field_EffectAhead - an effect object ahead: Sound_PlayEffect(0x10B), its +8
// = Sprite_Current's +8, its +0xA = 1, Sprite_Current +6 = its index, +3 one
// on (and one more below: two in all). None: the point two steps ahead; the
// object Sprite_ObjectAt(point, 0) finds gets bit 0 of its +0x80 and
// Sound_PlayEffect(0x10B); then PartyAction18_CellStrike on the point's cell,
// the cell one on in x (x with a fraction), one on in z (z with one), each
// only while the earlier found nothing; then +3 one on. Sprite_ScriptTickOnce
// every time. The effect index (a signed byte, 0..19 or 0xFF from
// Field_EffectAhead) and the object index are not checked by the original.
extern "C" void __cdecl PartyAction18_Form0Sub0Strike(void) {
    const char* const who = "PartyAction18_Form0Sub0Strike (0x524A60)";
    unsigned char* const s = Sc();
    if (s[0xA] != 0) {
        --s[0xA];
        if (s[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(W16(Word(s + 0x2C) + 0x100));
            const unsigned char ahead = SH_CALL(Field_EffectAhead)();
            if (ahead != 0xFF) {
                SH_CALL(Sound_PlayEffect)(0x10B);
                unsigned char* const c = Sc();
                // `movsx eax, bl; shl eax, 7`: 0x80..0xFE index before the
                // records; EffectAt aborts on any byte past 19
                unsigned char* const e = EffectAt(ahead, who);
                e[8] = c[8];
                e[0xA] = 1;
                c[6] = ahead;
                ++c[3];
            } else {
                const unsigned char* const c = Sc();
                const unsigned d = c[8];
                const U x = static_cast<U>(Long(c + 0x34)) + 2u * StepX(d);
                const U z = static_cast<U>(Long(c + 0x38)) + 2u * StepZ(d);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObject(found, who);
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                const unsigned cx = x >> 16, cz = z >> 16;
                if (SH_CALL(PartyAction18_CellStrike)(cx, cz) == 0) {
                    bool done = false;
                    if ((x & 0xFFFF) != 0) done = SH_CALL(PartyAction18_CellStrike)(cx + 1, cz) != 0;
                    if (!done && (z & 0xFFFF) != 0) SH_CALL(PartyAction18_CellStrike)(cx, cz + 1);
                }
            }
            ++Sc()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x524BB0 (0x188 bytes), set 18's strike on the map cell (x, z) -
// AreaMap_ByteAt:
//   0xF0, 0xF1, 0xF4 - Effect_SpawnAtCellHigh(0, x, z); when Rand & 7 is 6
//          or 7, Effect_SpawnAtCellHigh(4, x, z); Sound_PlayEffect(0x10B); 1.
//   0xF6, 0xF7 - Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B);
//          then Rand & 0xF: below 7 - Effect_SpawnAtCellHigh(3, x, z), item
//          0x29's name to Text_Records, Inventory_Add(0, 0x29, 1) taken:
//          Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); +0xB = 2. Above 0xB - Effect_SpawnAtCellHigh(2,
//          x, z), Sprite_FlashClut(0), Char_LoseHp(1, Field_State +0x89),
//          Msg_OpenSystem(0xD9), +0xB = 1. Either way (7..0xB too)
//          Field_Request = 2; 1.
//   else 0.
// The callers push a fourth dword to Effect_SpawnAtCellHigh (here the z
// argument's slot, its low byte overwritten by the cell); it reads three.
extern "C" unsigned char __cdecl PartyAction18_CellStrike(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 7;
        if (roll > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(0x10B);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(0x10B);
    const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x29);
        CopyName(name);
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Sc()[0xB] = 2;
        Field_Request = 2;
        return 1;
    }
    if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Field_State[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sc()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

// ===========================================================================
// A raised sprite's cell ahead (Field_CellAhead's other half)
// ===========================================================================

// original 0x5287B0 (0xC3 bytes): cells 1..7 - Field_CellKind of the cell (x,
// z) seen from (x0, z0); then of (x, zs), (x, zs + 1) and (x, zs - 1) from
// (x0, the same z) and of (xs, z), (xs + 1, z), (xs - 1, z) from (the same x,
// z0), zs / xs the sprite's own cell words +0x3A / +0x36 (16 bits each,
// Sprite_Current re-read before each): cells 1, 2, 3, 4, 5, 6, 7 in the order
// (x, z), (x, zs), (x, zs + 1), (xs, z), (xs + 1, z), (x, zs - 1), (xs - 1, z).
extern "C" void __cdecl Field_ReadCellsRaised(unsigned x, unsigned z, unsigned x0, unsigned z0) {
    Cell(1) = SH_CALL(Field_CellKind)(x, z, x0, z0);
    std::uint16_t zs = Word(Sc() + 0x3A);
    Cell(2) = SH_CALL(Field_CellKind)(x, zs, x0, zs);
    zs = W16(Word(Sc() + 0x3A) + 1);
    Cell(3) = SH_CALL(Field_CellKind)(x, zs, x0, zs);
    std::uint16_t xs = Word(Sc() + 0x36);
    Cell(4) = SH_CALL(Field_CellKind)(xs, z, xs, z0);
    xs = W16(Word(Sc() + 0x36) + 1);
    Cell(5) = SH_CALL(Field_CellKind)(xs, z, xs, z0);
    zs = W16(Word(Sc() + 0x3A) - 1);
    Cell(6) = SH_CALL(Field_CellKind)(x, zs, x0, zs);
    xs = W16(Word(Sc() + 0x36) - 1);
    Cell(7) = SH_CALL(Field_CellKind)(xs, z, xs, z0);
}

// original 0x527DB0 (0x23B bytes): the class of up to five cells (indices a..e
// into the cells, the list ending at the first 0), Field_CellClass's rules
// for a raised sprite: two 0x70 alone are 0x70; any 0x70 counts as 0xFF; any
// 0xFF is 0x10; all 0xB0 (or no cell) 0xB0. Two cells: 0xA0 in either, or
// 0xA2 in either with an even facing, 0x10; 0xA3 with 0xA3 0xA3, with any
// other 0x10; a 0xA_ in either, the first when both are equal, else 0x10.
// Three: 0xA3 anywhere 0x10, 0xA2 anywhere with an even facing 0x10; a 0xA_
// anywhere: the first and third read 0xA1 for 0xA0 (the third in the list
// itself), the answer the first when all three agree, else 0x10. One, four or
// five: any 0xA_ is 0x10. Otherwise 0x20 when a 0x2_ cell differs from a
// later 0x2_ (only the first 0x2_ is compared), else 0.
extern "C" unsigned char __cdecl Field_CellClass5(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e) {
    unsigned char k[5] = {static_cast<unsigned char>(a), static_cast<unsigned char>(b), static_cast<unsigned char>(c),
                          static_cast<unsigned char>(d), static_cast<unsigned char>(e)};
    unsigned n = 0;
    while (n < 5 && k[n] != 0) ++n;
    for (unsigned i = 0; i < n; ++i) k[i] = Cell(k[i]);
    if (n == 2 && k[0] == 0x70 && k[1] == 0x70) return 0x70;
    for (unsigned i = 0; i < n; ++i)
        if (k[i] == 0x70) k[i] = 0xFF;
    for (unsigned i = 0; i < n; ++i)
        if (k[i] == 0xFF) return 0x10;
    {
        unsigned i = 0;
        while (i < n && k[i] == 0xB0) ++i;
        if (i == n) return 0xB0;
    }
    if (n == 2) {
        const unsigned char k0 = k[0], k1 = k[1];
        if (k0 == 0xA0 || k1 == 0xA0) return 0x10;
        if ((k0 == 0xA2 || k1 == 0xA2) && (Sc()[8] & 1) == 0) return 0x10;
        if (k0 == 0xA3) return k1 == 0xA3 ? 0xA3 : 0x10;
        if ((k0 & 0xF0) == 0xA0 || (k1 & 0xF0) == 0xA0) return k0 == k1 ? k0 : 0x10;
    } else if (n == 3) {
        const unsigned char k0 = k[0], k1 = k[1], k2 = k[2];
        if (k0 == 0xA3 || k1 == 0xA3 || k2 == 0xA3) return 0x10;
        if ((k0 == 0xA2 || k1 == 0xA2 || k2 == 0xA2) && (Sc()[8] & 1) == 0) return 0x10;
        if ((k0 & 0xF0) == 0xA0 || (k1 & 0xF0) == 0xA0 || (k2 & 0xF0) == 0xA0) {
            const unsigned char first = k0 == 0xA0 ? 0xA1 : k0;
            if (k2 == 0xA0) k[2] = 0xA1;
            return first == k1 && first == k[2] ? first : 0x10;
        }
    } else {
        for (unsigned i = 0; i < n; ++i)
            if ((k[i] & 0xF0) == 0xA0) return 0x10;
    }
    for (unsigned i = 0; i < n; ++i) {
        if ((k[i] & 0xF0) != 0x20) continue;
        for (unsigned j = i + 1; j < n; ++j)
            if ((k[j] & 0xF0) == 0x20 && k[i] != k[j]) return 0x20;
        return 0;
    }
    return 0;
}

// original 0x527FF0 (0x74 bytes): with cell 8 a 0x10 or 0x20 class - cells 9
// and 10 both 0x10 / 0x20: 1; only cell 9: Field_TurnUnless(2, 4, 3); only cell
// 10: Field_TurnUnless(4, 6, 5); neither: +8 one eighth on (& 7). 0 but for
// the first case; 0 when cell 8 is neither.
extern "C" unsigned char __cdecl Field_CornerTurn(void) {
    const unsigned char c8 = Cell(8);
    if (c8 != 0x10 && c8 != 0x20) return 0;
    const unsigned char c9 = Cell(9), c10 = Cell(10);
    const bool side9 = c9 == 0x10 || c9 == 0x20, side10 = c10 == 0x10 || c10 == 0x20;
    if (side9 && side10) return 1;
    if (side9) {
        SH_CALL(Field_TurnUnless)(2, 4, 3);
        return 0;
    }
    if (side10) {
        SH_CALL(Field_TurnUnless)(4, 6, 5);
        return 0;
    }
    unsigned char* const s = Sc();
    s[8] = static_cast<unsigned char>((s[8] + 1) & 7);
    return 0;
}

// original 0x5280A0 (0x4F bytes): MapView_SlopeAt at the point half way
// between two cells - x the sum of the words x0 and x1 (sign-extended) as
// 16.16, halved toward zero, z likewise - in `direction` (passed whole); 1
// when the sloped flag (cell 0) is set and the answer's low word, signed, is
// above 0x40, else 0.
extern "C" unsigned char __cdecl Field_SlopeBetween(unsigned x0, unsigned x1, unsigned z0, unsigned z1, unsigned direction) {
    const long x = Half(static_cast<U>(static_cast<int>(static_cast<short>(x0)) + static_cast<int>(static_cast<short>(x1))) << 16);
    const long z = Half(static_cast<U>(static_cast<int>(static_cast<short>(z0)) + static_cast<int>(static_cast<short>(z1))) << 16);
    const long r = SH_CALL(MapView_SlopeAt)(x, z, direction);
    if (Cell(0) == 0) return 0;
    return static_cast<short>(r) > 0x40 ? 1 : 0;
}

// original 0x528190 (0x122 bytes): a straight-facing raised sprite on a
// cell's corner turned along a slope - in order, each only while +8 (re-read)
// is even: Field_SlopeBetween(x, x, zs, zs + 1, 1) turns it (4, 6, 5); then
// between (x, x) and (zs, z) for facings without bit 2, (z, zs + 1) with it,
// in direction 1, turns it (4, 6, 5); then (xs, xs + 1, z, z, 3) turns it (2,
// 4, 3); then (xs, x, z, z, 3) for facing 0 or 6, else (xs + 1, x, z, z, 3),
// turns it (2, 4, 3). xs / zs the sprite's cell words +0x36 / +0x3A.
extern "C" void __cdecl Field_RaisedEdgeTurns(unsigned x, unsigned z) {
    if ((Sc()[8] & 1) == 0) {
        const std::uint16_t zs = Word(Sc() + 0x3A);
        if (SH_CALL(Field_SlopeBetween)(x, x, zs, W16(zs + 1u), 1) != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
    }
    {
        const unsigned char* const s = Sc();
        const unsigned char facing = s[8];
        if ((facing & 1) == 0) {
            unsigned char r;
            if ((facing & 4) == 0) r = SH_CALL(Field_SlopeBetween)(x, x, Word(s + 0x3A), z, 1);
            else r = SH_CALL(Field_SlopeBetween)(x, x, z, W16(Word(s + 0x3A) + 1u), 1);
            if (r != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
        }
    }
    {
        const unsigned char* const s = Sc();
        if ((s[8] & 1) == 0) {
            const std::uint16_t xs = Word(s + 0x36);
            if (SH_CALL(Field_SlopeBetween)(xs, W16(xs + 1u), z, z, 3) != 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
        }
    }
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    if (facing & 1) return;
    unsigned char r;
    if (facing == 0 || facing == 6) r = SH_CALL(Field_SlopeBetween)(Word(s + 0x36), x, z, z, 3);
    else r = SH_CALL(Field_SlopeBetween)(W16(Word(s + 0x36) + 1u), x, z, z, 3);
    if (r != 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
}

namespace {

// Field_CellAheadRaised's diagonal side tests (0x527AAF and 0x527C30 in the
// original): the class of three cells into cell 8 and three single cells into
// 9..0xB; then the answers as the original orders them. `turn_pair` and
// `turn_a2` are the two Field_CellPairTurn directions, `turn_a2_other` the
// one used when cell 0xB is 0xA2; `second` the cell the 0x20 class checks.
struct DiagSide {
    unsigned c0, c1, c2;        // the three cells of the class into cell 8
    unsigned second;            // cell 2 or 4: a 0x2_ there makes 0x20 a stop
    unsigned turn, turn_a2;     // Field_CellPairTurn's `to` for 0x10 / 0xA2 (cell 0xB not 0xA2), and for 0xA2 (cell 0xB 0xA2)
    unsigned to_first, to_second;   // the facing set when the first / second slope probe answers
};

unsigned char DiagEdge(const DiagSide& d, bool on_x, std::uint16_t ax, std::uint16_t az) {
    Cell(8) = SH_CALL(Field_CellClass5)(d.c0, d.c1, d.c2, 0, 0);
    Cell(9) = SH_CALL(Field_CellClass)(d.c0, 0, 0);
    Cell(10) = SH_CALL(Field_CellClass)(d.c2, 0, 0);
    const unsigned char last = SH_CALL(Field_CellClass)(d.c1, 0, 0);
    Cell(0xB) = last;
    unsigned char k = Cell(8);
    if (k == 0xB0) return 6;
    if (k == 0x70) return 7;
    bool pair = k == 0x10;
    if (k == 0x20) {
        if ((Cell(d.second) & 0xF0) == 0x20) return 0;
        pair = true;
    }
    if (pair) {
        if (last == 0x10) return 0;
        if (SH_CALL(Field_CellPairTurn)(9, 0xA, d.turn, 0x10) != 0) return 0;
        if (Cell(0xB) == 0xA2) {
            if (SH_CALL(Field_CellPairTurn)(9, 0xA, d.turn_a2, 0xA2) != 0) return 0;
        } else {
            if (SH_CALL(Field_CellPairTurn)(9, 0xA, d.turn, 0xA2) != 0) return 0;
        }
        k = Cell(8);
    }
    if ((k & 0xF0) == 0xA0) return SH_CALL(Field_CellSlope)(8);
    if (on_x) {
        // the z side: between (ax, ax) and the sprite's zs and zs + 1, then zs - 1, in direction 1
        std::uint16_t zs = Word(Sc() + 0x3A);
        if (SH_CALL(Field_SlopeBetween)(ax, ax, zs, W16(zs + 1u), 1) != 0) {
            Sc()[8] = static_cast<unsigned char>(d.to_first);
            return 1;
        }
        zs = Word(Sc() + 0x3A);
        if (SH_CALL(Field_SlopeBetween)(ax, ax, zs, W16(zs - 1u), 1) == 0) return 3;
        Sc()[8] = static_cast<unsigned char>(d.to_second);
        return 1;
    }
    std::uint16_t xs = Word(Sc() + 0x36);
    if (SH_CALL(Field_SlopeBetween)(xs, W16(xs + 1u), az, az, 3) != 0) {
        Sc()[8] = static_cast<unsigned char>(d.to_first);
        return 1;
    }
    xs = Word(Sc() + 0x36);
    if (SH_CALL(Field_SlopeBetween)(xs, W16(xs - 1u), az, az, 3) == 0) return 3;
    Sc()[8] = static_cast<unsigned char>(d.to_second);
    return 1;
}

}  // namespace

// original 0x527640 (0x766 bytes): what the cell ahead of a raised sprite
// (+0x70 set) means for a step - Field_CellAhead tail-jumps here, as it does
// to Field_CellAheadFlat for a grounded one. 1 at once when
// Field_ScriptFlags has bit 10, or when the sprite stands on a cell's corner
// (both 16.16 fractions 0: the opposite of the flat half's test). Otherwise
// the cell ahead from Field_CellOffsets' two signed bytes for +8 (an offset
// of 1 counts 2; the cell stepped from is the sprite's own, one on where the
// offset is 1), Field_ReadCellsRaised; then by the facing (re-read) and the
// fractions the classes into cells 8..0xB and the answer - 0 stop, 1 go on
// (turned), 3 nothing known in the way, 6 / 7 a state change, or
// Field_CellSlope's - each step as docs/rest_1f.md section 1.3 lists it.
extern "C" unsigned char __cdecl Field_CellAheadRaised(void) {
    if (Field_ScriptFlags & 0x400) return 1;
    const unsigned char* c = Sc();
    const unsigned char facing = c[8];
    if (Word(c + 0x34) == 0 && Word(c + 0x38) == 0) return 1;
    const unsigned char* const offsets = At(kCellOffsets) + static_cast<unsigned>(facing) * 2u;
    const auto ox = static_cast<signed char>(offsets[0]);
    const auto oz = static_cast<signed char>(offsets[1]);
    const std::uint16_t dx = ox == 1 ? 2 : static_cast<std::uint16_t>(static_cast<short>(ox));
    const std::uint16_t dz = oz == 1 ? 2 : static_cast<std::uint16_t>(static_cast<short>(oz));
    const std::uint16_t xs = Word(c + 0x36), zs = Word(c + 0x3A);
    const std::uint16_t ax = W16(xs + dx), az = W16(zs + dz);
    SH_CALL(Field_ReadCellsRaised)(ax, az, W16(xs + (ox == 1 ? 1u : 0u)), W16(zs + (oz == 1 ? 1u : 0u)));
    c = Sc();
    const unsigned char dir = c[8];
    if ((dir & 1) == 0) {
        // Straight.
        if (Word(c + 0x34) != 0) {
            if (Word(c + 0x38) != 0) {
                // Mid-cell on both axes: ahead and the two sides.
                Cell(8) = SH_CALL(Field_CellClass5)(1, 2, 3, 4, 5);
                Cell(9) = SH_CALL(Field_CellClass5)(4, 5, 0, 0, 0);
                const unsigned char side_z = SH_CALL(Field_CellClass5)(2, 3, 0, 0, 0);
                Cell(10) = side_z;
                if (Cell(8) == 0xB0) return 6;
                if (Cell(9) == 0x70) {
                    SH_CALL(Field_TurnUnless)(4, 6, 5);
                    return 7;
                }
                if (side_z == 0x70) {
                    SH_CALL(Field_TurnUnless)(2, 4, 3);
                    return 7;
                }
                if (SH_CALL(Field_CornerTurn)() != 0) return 0;
                const unsigned char high9 = Cell(9) & 0xF0;
                unsigned char* const s = Sc();
                if (high9 == 0xA0 && (s[8] == 1 || s[8] == 5)) return SH_CALL(Field_CellSlope)(9);
                const unsigned char high10 = Cell(10) & 0xF0;
                if (high10 == 0xA0 && (s[8] == 3 || s[8] == 7)) return SH_CALL(Field_CellSlope)(0xA);
                if (high9 == 0xA0) {
                    s[8] = facing;
                    SH_CALL(Field_TurnUnless)(4, 6, 5);
                    return SH_CALL(Field_CellSlope)(9);
                }
                if (high10 == 0xA0) {
                    s[8] = facing;
                    SH_CALL(Field_TurnUnless)(2, 4, 3);
                    return SH_CALL(Field_CellSlope)(0xA);
                }
                SH_CALL(Field_RaisedEdgeTurns)(ax, az);
                return 3;
            }
            // x mid-cell, z on a cell edge: ahead on x.
            unsigned char k = SH_CALL(Field_CellClass5)(3, 2, 6, 0, 0);
            Cell(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x10 || k == 0x20) {
                SH_CALL(Field_TurnUnless)(4, 6, 5);
                k = Cell(8);
            }
            if ((k & 0xF0) == 0xA0) {
                if ((Sc()[8] & 1) == 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
                return SH_CALL(Field_CellSlope)(8);
            }
            if (Sc()[8] & 1) return 3;
            std::uint16_t z0 = Word(Sc() + 0x3A);
            if (SH_CALL(Field_SlopeBetween)(ax, ax, z0, W16(z0 + 1u), 1) != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
            if (Sc()[8] & 1) return 3;
            z0 = Word(Sc() + 0x3A);
            if (SH_CALL(Field_SlopeBetween)(ax, ax, z0, W16(z0 - 1u), 1) != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
            return 3;
        }
        // x on a cell edge, z mid-cell: ahead on z.
        unsigned char k = SH_CALL(Field_CellClass5)(5, 4, 7, 0, 0);
        Cell(8) = k;
        if (k == 0xB0) return 6;
        if (k == 0x10 || k == 0x20) {
            SH_CALL(Field_TurnUnless)(2, 4, 3);
            k = Cell(8);
        }
        const unsigned char now = Sc()[8];
        if ((k & 0xF0) == 0xA0) {
            if ((now & 1) == 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
            return SH_CALL(Field_CellSlope)(8);
        }
        if (now & 1) return 3;
        std::uint16_t x0 = Word(Sc() + 0x36);
        if (SH_CALL(Field_SlopeBetween)(x0, W16(x0 + 1u), az, az, 3) != 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
        if (Sc()[8] & 1) return 3;
        x0 = Word(Sc() + 0x36);
        if (SH_CALL(Field_SlopeBetween)(x0, W16(x0 - 1u), az, az, 3) == 0) return 3;
        SH_CALL(Field_TurnUnless)(2, 4, 3);
        return 3;
    }

    // Diagonal.
    if (Word(c + 0x34) != 0) {
        if (Word(c + 0x38) != 0) {
            const unsigned char k = (dir & 2) ? SH_CALL(Field_CellClass5)(2, 3, 0, 0, 0) : SH_CALL(Field_CellClass5)(4, 5, 0, 0, 0);
            Cell(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x70) return 7;
            if (k == 0x10 || k == 0x20) return 0;
            if ((k & 0xF0) == 0xA0) return SH_CALL(Field_CellSlope)(8);
            const unsigned char* const s = Sc();
            unsigned char r;
            if (s[8] & 2) {
                const std::uint16_t z0 = Word(s + 0x3A);
                r = SH_CALL(Field_SlopeBetween)(ax, ax, z0, W16(z0 + 1u), 1);
            } else {
                const std::uint16_t x0 = Word(s + 0x36);
                r = SH_CALL(Field_SlopeBetween)(x0, W16(x0 + 1u), az, az, 3);
            }
            return r != 0 ? 0 : 3;
        }
        // x mid-cell, z on an edge: only facings with bit 1 (3, 7) look.
        if ((dir & 2) == 0) return 3;
        static const DiagSide kXSide = {3, 2, 6, 2, 1, 5, 1, 5};
        return DiagEdge(kXSide, true, ax, az);
    }
    // x on an edge, z mid-cell: only facings without bit 1 (1, 5) look.
    if (dir & 2) return 3;
    static const DiagSide kZSide = {5, 4, 7, 4, 7, 3, 7, 3};
    return DiagEdge(kZSide, false, ax, az);
}

// ===========================================================================
// The leader's state 9, stage 0
// ===========================================================================

// original 0x5288C0 (0x75 bytes), LeaderPanel_Stage0Steps[0]: the panel's
// pose byte 0x6BC716, its counter 0x6BC709 and the blink switch 0x939A28
// cleared; +0x29 = 5; the height word +0x3E = AreaMap_Elevation(+0x34, +0x38);
// Field_State +0x89 0: Sprite_SetAnimationAt(0xA, 2), else
// Sprite_SetAnimation(0xB); +3 one on.
extern "C" void __cdecl LeaderPanel_S0Begin(void) {
    At(kPanelPose)[0] = 0;
    At(kPanelCounter)[0] = 0;
    At(kPanelBlink)[0] = 0;
    Sc()[0x29] = 5;
    const unsigned char* const s = Sc();
    const long height = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetWord(Sc() + 0x3E, static_cast<unsigned>(height));
    if (Field_State[0x89] == 0) SH_CALL(Sprite_SetAnimationAt)(0xA, 2);
    else SH_CALL(Sprite_SetAnimation)(0xB);
    ++Sc()[3];
}

// original 0x528940 (0x26 bytes), LeaderPanel_Stage0Steps[1]: once the wait
// word MoveScript_WaitWordDA is 0, effect record 4's +6 = 0 and its state +1
// one on, and +3 one on.
extern "C" void __cdecl LeaderPanel_S0Wait(void) {
    if (MoveScript_WaitWordDA != 0) return;
    unsigned char* const record4 = Effect_Objects + 4 * kEffectStride;
    const unsigned char state = record4[1];
    record4[6] = 0;
    record4[1] = static_cast<unsigned char>(state + 1);
    ++Sc()[3];
}

// original 0x528970 (0x2C bytes), LeaderPanel_Stage0Steps[2]: once effect
// record 4's state +1 is 4, record 1's state +1 one on, +2 one on (stage 1)
// and +3 = 0.
extern "C" void __cdecl LeaderPanel_S0End(void) {
    if (Effect_Objects[4 * kEffectStride + 1] != 4) return;
    ++Effect_Objects[kEffectStride + 1];
    ++Sc()[2];
    Sc()[3] = 0;
}

void Rest1F_Inject() {
    if (bof3::WantsShadow("rest_1f")) rest_1f::SelfTest();
    BOF3_INJECT(PartyAction16_FormAction);
    BOF3_INJECT(PartyAction16_ByForm);
    BOF3_INJECT(PartyAction16_FormAction2);
    BOF3_INJECT(PartyAction16_Form2);
    BOF3_INJECT(PartyAction16_Form2Begin);
    BOF3_INJECT(PartyAction16_Form2Resolve);
    BOF3_INJECT(PartyAction16_CellPickup);
    BOF3_INJECT(PartyAction17_FormAction);
    BOF3_INJECT(PartyAction17_ByForm);
    BOF3_INJECT(PartyAction17_FormAction0);
    BOF3_INJECT(PartyAction17_FormAction1);
    BOF3_INJECT(PartyAction17_FormAction2);
    BOF3_INJECT(PartyAction17_Form0);
    BOF3_INJECT(PartyAction17_Form1);
    BOF3_INJECT(PartyAction17_Form2);
    BOF3_INJECT(PartyAction17_Form1Begin);
    BOF3_INJECT(PartyAction17_Form1Resolve);
    BOF3_INJECT(PartyAction17_CellPickup);
    BOF3_INJECT(PartyAction18_FormAction);
    BOF3_INJECT(PartyAction18_ByForm);
    BOF3_INJECT(PartyAction18_FormAction0);
    BOF3_INJECT(PartyAction18_FormAction1);
    BOF3_INJECT(PartyAction18_FormAction2);
    BOF3_INJECT(PartyAction18_Form0);
    BOF3_INJECT(PartyAction18_Form0Sub0);
    BOF3_INJECT(PartyAction18_Form0Sub1);
    BOF3_INJECT(PartyAction18_Form0Sub0Begin);
    BOF3_INJECT(PartyAction18_Form0Sub0Strike);
    BOF3_INJECT(PartyAction18_CellStrike);
    BOF3_INJECT(PartyAction18_ProbeStart);
    BOF3_INJECT(PartyAction18_Form1);
    BOF3_INJECT(PartyAction18_Form1Begin);
    BOF3_INJECT(PartyAction18_Form1Resolve);
    BOF3_INJECT(PartyAction18_CellPickup);
    BOF3_INJECT(PartyAction18_Form2);
    BOF3_INJECT(PartyAction_ProbeStart);
    BOF3_INJECT(PartyAction_EffectCountdown);
    BOF3_INJECT(PartyAction_SpawnKind1B);
    BOF3_INJECT(Field_CellAheadRaised);
    BOF3_INJECT(Field_CellClass5);
    BOF3_INJECT(Field_CornerTurn);
    BOF3_INJECT(Field_SlopeBetween);
    BOF3_INJECT(Field_RaisedEdgeTurns);
    BOF3_INJECT(Field_ReadCellsRaised);
    BOF3_INJECT(LeaderPanel_Run);
    BOF3_INJECT(LeaderPanel_S0);
    BOF3_INJECT(LeaderPanel_S0Begin);
    BOF3_INJECT(LeaderPanel_S0Wait);
    BOF3_INJECT(LeaderPanel_S0End);
}
