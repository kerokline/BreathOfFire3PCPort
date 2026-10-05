// Round fourteen group R1E (docs/rest_1e.md): the 47 functions of
// analysis/round14_cut.tsv's group R1E, 0x5226D0..0x523EC2, each read with
// capstone to its last instruction (2026-10-04). The field actions of party
// sets 13, 14 and 15 and of set 16's forms 0 and 1: Field_ActionBySet /
// Field_FormActions (by the party set) reach a set's dispatcher, which jumps
// through the set's forms table by the sprite's u16 +0x2C; a form's dispatcher
// jumps through its states table by +2 (two forms: through a modes table by +2,
// then a mode's states by +3); the states below. The pattern repeats per set:
// the same code at two or three addresses (capstone, the constants equal), so
// one body serves each shape and the copies differ only in which of the
// group's cell helpers they call.
//
// Every call goes through the scenario harness (SH_CALL), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps through
// a table past its end, or indexes Effect_Objects or the sprite records past
// them by a byte it was handed, ours aborts with a message (docs/rest_1e.md
// section 5).
#include "game/rest_1e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_1e::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using Handler = scenario_harness::Handler;
using CellHelper = unsigned char(__cdecl*)(unsigned, unsigned);

constexpr unsigned kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;

unsigned char* S() { return Sprite_Current; }
unsigned char Al(U eax) { return static_cast<unsigned char>(eax); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + k] (or mov ax, [ecx +
// 0x2C]); jmp [table + eax * 4]: the entry read in place (the fuzz swaps the
// cells for recorders). No compare bounds the index; ours aborts past the
// table's own length, where the original jumps through the next table's dwords.
void Dispatch(const char* who, const char* by, U table, unsigned index, unsigned entries) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1e.md section 5)",
                    who, by, index, entries, static_cast<unsigned>(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))();
}
// A state table's address (symbols.toml [[data]]: the name is a pointer to its
// first dword).
U Table(const unsigned long* t) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(t)); }
void ByState2(const char* who, U table, unsigned entries) { Dispatch(who, "the state byte +2", table, S()[2], entries); }
void ByState3(const char* who, U table, unsigned entries) { Dispatch(who, "the state byte +3", table, S()[3], entries); }
void ByForm(const char* who, U table) { Dispatch(who, "the form word +0x2C", table, Word(S() + 0x2C), 3); }

// Field_DirectionSteps' row for a direction, read in place with the direction
// unmasked, as the originals index it.
U StepX(unsigned d) { return static_cast<U>(Long(At(at::kSteps + d * 8))); }
U StepZ(unsigned d) { return static_cast<U>(Long(At(at::kSteps + d * 8 + 4))); }

// The animation offset of a direction as the originals compute it: dec eax;
// cdq; sub eax, edx; sar eax, 1 on the zero-extended byte - (d - 1) / 2
// truncated, so 0 gives 0; the add to al never carries (0..0x7F + 0x46).
unsigned HalfTurn(unsigned char d) { return static_cast<unsigned>((static_cast<int>(d) - 1) / 2); }

// An even direction turned one eighth back, then two on, then back again,
// while `probe` (PartyAction_TargetAhead or PartyAction_BlockedAhead) answers
// 0 - each turn on Sprite_Current re-read after the call. An odd direction is
// kept, unmasked.
void TurnWhileClear(unsigned char (__cdecl* probe)(void)) {
    unsigned char* const s = S();
    if ((s[8] & 1) != 0) return;
    s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
    if (Al(SH_CALL(probe)()) != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] + 2) & 7);
    if (Al(SH_CALL(probe)()) != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] - 2) & 7);
}

// A side probe in direction d (3 or 5, the immediate the originals push; the
// row read by its address): the point one step that way from Sprite_Current;
// when MapView_SlopeAt there is steep (the scratch flag set and its low word,
// signed, above 0x40) and MapView_GroundAt there is above the height word
// (s16 against s16), +0x2B = 0. PartyAction_SideProbes' probe, inline.
void SideProbe(unsigned d, U row) {
    const unsigned char* const s = S();
    const U x = static_cast<U>(Long(s + 0x34)) + static_cast<U>(Long(At(row)));
    const U z = static_cast<U>(Long(s + 0x38)) + static_cast<U>(Long(At(row + 4)));
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (At(bof3::addr::DamageScratch)[0] == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    unsigned char* const c = S();
    if (static_cast<short>(Word(c + 0x3E)) < static_cast<short>(ground)) c[0x2B] = 0;
}
void SideProbes() {
    S()[0x2B] = 1;
    SideProbe(3, at::kStepRow3);
    SideProbe(5, at::kStepRow5);
}

// Sprite_ObjectAt's index, as the originals use it: bit 0 of the object's
// +0x80 - 0..0x1D a Sprite_Objects record, 0x1E and up (cmp al, 0x1E; jge, the
// byte signed) Sprite_ObjectsExtra's. The real one answers 0..0x21 or 0xFF; ours
// aborts on any other byte, which the original would use to write outside the
// records.
void FlagObject(const char* who, unsigned char found) {
    const int i = static_cast<signed char>(found);
    if (i < 0 || i >= static_cast<int>(at::kObjectCount + at::kExtraCount))
        bof3::Fatal("%s: Sprite_ObjectAt answered 0x%02X, past the 0x22 sprite records; the original sets bit 0 of a "
                    "byte outside them (docs/rest_1e.md section 5)",
                    who, static_cast<unsigned>(found));
    if (i < static_cast<int>(at::kObjectCount))
        At(at::kObjectFlag + static_cast<U>(i) * at::kObjectStride)[0] |= 1;
    else
        At(at::kExtraFlag + static_cast<U>(i - static_cast<int>(at::kObjectCount)) * at::kObjectStride)[0] |= 1;
}

// An Effect_Objects record by an index the original uses unchecked; ours aborts
// past the 20 records.
unsigned char* EffectAt(const char* who, int index, const char* what) {
    if (index < 0 || index >= static_cast<int>(kEffectCount))
        bof3::Fatal("%s: %s is %d, past Effect_Objects' 20 records; the original reads 0x%X unchecked "
                    "(docs/rest_1e.md section 5)",
                    who, what, index, static_cast<unsigned>(0x7E11E0 + index * static_cast<int>(kEffectStride)));
    return Effect_Objects + index * static_cast<int>(kEffectStride);
}

// The point two steps ahead of Sprite_Current (read now), as the resolve and
// strike states compute it (lea r, [pos + step * 2]).
void TwoAhead(U& x, U& z) {
    const unsigned char* const s = S();
    const unsigned d = s[8];
    x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
    z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
}

// The cell calls after a probe two steps ahead: on the point's cell; when that
// answers 0 and x has a fraction, on the cell one on in x; when still 0 and z
// has one, on the cell one on in z. The originals pass dwords whose low words
// are the cells and whose upper halves are their own uninitialised stack;
// every callee reads the low word only (docs/rest_1e.md section 2).
void CellsAhead(CellHelper helper, U x, U z) {
    const unsigned cx = x >> 16, cz = z >> 16;
    if (Al(SH_CALL(helper)(cx, cz)) != 0) return;
    if ((x & 0xFFFF) != 0 && Al(SH_CALL(helper)(cx + 1, cz)) != 0) return;
    if ((z & 0xFFFF) != 0) SH_CALL(helper)(cx, cz + 1);
}

// --- the shapes' bodies -------------------------------------------------------------

// 0x522760 / 0x5230D0 / 0x523550.
void Form2Begin() {
    TurnWhileClear(&PartyAction_TargetAhead);
    const unsigned char* s = S();
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = S();
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(s + 0x3E)));
    // push eax with al the direction and the rest of eax Sprite_Current itself.
    // The upper bits pass through an empty asm: without it this toolchain's
    // x86 back end dropped them (the IR keeps the `or`; the code pushed the
    // byte alone - 12,000 mismatches, docs/rest_1e.md section 4).
    U upper = static_cast<U>(reinterpret_cast<std::uintptr_t>(s)) & 0xFFFFFF00u;
    asm volatile("" : "+r"(upper));
    const U direction = upper | s[8];
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), direction);   // its answer unused
    if (At(bof3::addr::DamageScratch)[0] != 0 && rise > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x46));
        ++S()[2];
    } else {
        SideProbes();
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(S() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
        S()[0xA] = 5;
    }
    S()[0xB] = 0;
    ++S()[2];
}

// 0x522940 / 0x5232B0 / 0x523730: PartyAction5_Form0Resolve's code calling the
// set's own cell pickup.
void Form2Resolve(const char* who, CellHelper pickup) {
    --S()[0xA];
    if (S()[0xA] == 0) {
        U x, z;
        TwoAhead(x, z);
        const unsigned char found = Al(SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0));
        if (found != 0xFF) FlagObject(who, found);
        CellsAhead(pickup, x, z);
        ++S()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x522A20 / 0x523390 / 0x523810: Field_CellPickup's code, the bonus x 20.
unsigned char CellPickup(unsigned x, unsigned z) {
    const unsigned char cell = Al(SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)));
    if (cell == 0xF2) {
        if (Al(SH_CALL(Effect_FindFree)()) != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = Al(static_cast<U>(SH_CALL(Rand)())) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (Al(static_cast<U>(SH_CALL(Rand)())) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 20);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            S()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, at::kPickupItem);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
        if (Al(SH_CALL(Inventory_Add)(0, at::kPickupItem, 1)) != 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundTaken);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        S()[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x522C00 / 0x523B40.
void StrikeBegin() {
    TurnWhileClear(&PartyAction_BlockedAhead);
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xB] = 0;
    S()[0xA] = 0xB;
    S()[3] = 1;
}

// 0x522C90 / 0x523BD0.
void Strike(const char* who, CellHelper strike) {
    const unsigned char t = S()[0xA];
    if (t != 0) {
        S()[0xA] = static_cast<unsigned char>(t - 1);
        if (S()[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(S() + 0x2C) + 0x100));
            const unsigned char ahead = Al(SH_CALL(Field_EffectAhead)());
            if (ahead != 0xFF) {
                SH_CALL(Sound_PlayEffect)(at::kSoundStrike);
                unsigned char* const s = S();
                unsigned char* const e = EffectAt(who, static_cast<signed char>(ahead), "Field_EffectAhead's answer");
                e[8] = s[8];
                e[0xA] = 1;
                s[6] = ahead;
                ++S()[3];
            } else {
                U x, z;
                TwoAhead(x, z);
                const unsigned char found = Al(SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0));
                if (found != 0xFF) {
                    FlagObject(who, found);
                    SH_CALL(Sound_PlayEffect)(at::kSoundStrike);
                }
                CellsAhead(strike, x, z);
            }
            ++S()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x522E20 / 0x523D20. Effect_SpawnAtCellHigh is pushed a fourth dword (the
// z argument's slot after the cell byte was stored into its low byte), which it
// never reads; ours passes the three it reads.
unsigned char StrikeCell(unsigned x, unsigned z) {
    const unsigned char cell = Al(SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if (static_cast<signed char>(Al(static_cast<U>(SH_CALL(Rand)())) & 7) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(at::kSoundStrike);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(at::kSoundStrike);
    const unsigned roll = Al(static_cast<U>(SH_CALL(Rand)())) & 0xF;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, at::kStrikeItem);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
        if (Al(SH_CALL(Inventory_Add)(0, at::kStrikeItem, 1)) != 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundTaken);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        S()[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        const unsigned member = Field_State[0x89];
        SH_CALL(Char_LoseHp)(1, member);
        SH_CALL(Msg_OpenSystem)(at::kStrikeHurtMessage);
        S()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

}  // namespace

// ===========================================================================
// Shared by many sets
// ===========================================================================

// original 0x5226D0 (0xD bytes): Field_State +0x137 = 0.
extern "C" void __cdecl PartyAction_NoAction(void) { Field_State[0x137] = 0; }

// original 0x5239F0 (0xE7 bytes): an even +8 turned one eighth back (& 7, an
// odd one kept unmasked); +0x2B = 1 and the side probes in directions 3 and 5;
// Sprite_EnsureAnimation((+8 >> 1) + 0x42, a byte: shr cl, 1; add cl, 0x42 -
// the rest of the pushed ecx is Sprite_Current's, which the callee does not
// read); +2 one on.
extern "C" void __cdecl PartyAction_ProbeBegin(void) {
    unsigned char* const s = S();
    if ((s[8] & 1) == 0) s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
    SideProbes();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>((S()[8] >> 1) + 0x42));
    ++S()[2];
}

// original 0x522DE0 (0x38 bytes): the effect object +6 names (the byte
// zero-extended, unchecked: ours aborts past the 20 records) free (+0 zero) or
// in state 1 (+1), and Field_Kind2Hold 0: +6 = 0, +3 = +3 - 2.
extern "C" void __cdecl PartyAction_StrikeWait(void) {
    unsigned char* const s = S();
    const unsigned char* const e = EffectAt("PartyAction_StrikeWait (0x522DE0)", s[6], "the sprite's +6");
    if (e[0] != 0 && e[1] != 1) return;
    if (Field_Kind2Hold != 0) return;
    s[6] = 0;
    S()[3] = static_cast<unsigned char>(S()[3] - 2);
}

// ===========================================================================
// The dispatchers (0x12 / 0x13 bytes each): mov ecx, [Sprite_Current]; xor
// eax, eax; mov al, [ecx + 2 or 3] / mov ax, [ecx + 0x2C]; jmp [table + eax *
// 4]. The table's length is the run of code pointers to the next table (no
// reader bounds it; docs/rest_1e.md section 3).
// ===========================================================================

extern "C" void __cdecl PartyFormAction13_ByForm(void) { ByForm("PartyFormAction13_ByForm (0x522B40)", Table(PartyFormAction13_Forms)); }
extern "C" void __cdecl PartyAction13_ByForm(void) { ByForm("PartyAction13_ByForm (0x522B60)", Table(PartyAction13_Forms)); }
extern "C" void __cdecl PartyFormAction13_Form1(void) { ByState2("PartyFormAction13_Form1 (0x5226E0)", Table(PartyFormAction13_Form1States), 3); }
extern "C" void __cdecl PartyAction13_Form1(void) { ByState2("PartyAction13_Form1 (0x522700)", Table(PartyAction13_Form1States), 3); }
extern "C" void __cdecl PartyFormAction13_Form2(void) { ByState2("PartyFormAction13_Form2 (0x522720)", Table(PartyFormAction13_Form2States), 3); }
extern "C" void __cdecl PartyAction13_Form2(void) { ByState2("PartyAction13_Form2 (0x522740)", Table(PartyAction13_Form2States), 3); }

extern "C" void __cdecl PartyFormAction14_ByForm(void) { ByForm("PartyFormAction14_ByForm (0x5234B0)", Table(PartyFormAction14_Forms)); }
extern "C" void __cdecl PartyAction14_ByForm(void) { ByForm("PartyAction14_ByForm (0x5234D0)", Table(PartyAction14_Forms)); }
extern "C" void __cdecl PartyFormAction14_Form0(void) { ByState2("PartyFormAction14_Form0 (0x522B80)", Table(PartyFormAction14_Form0States), 3); }
extern "C" void __cdecl PartyFormAction14_Form1(void) { ByState2("PartyFormAction14_Form1 (0x522BA0)", Table(PartyFormAction14_Form1States), 3); }
extern "C" void __cdecl PartyFormAction14_Form2(void) { ByState2("PartyFormAction14_Form2 (0x523090)", Table(PartyFormAction14_Form2States), 3); }
extern "C" void __cdecl PartyAction14_Form1(void) { ByState2("PartyAction14_Form1 (0x522BC0)", Table(PartyAction14_Form1Modes), 2); }
extern "C" void __cdecl PartyAction14_Form1Mode0(void) { ByState3("PartyAction14_Form1Mode0 (0x522BE0)", Table(PartyAction14_Mode0States), 5); }
extern "C" void __cdecl PartyAction14_Form1Mode1(void) { ByState3("PartyAction14_Form1Mode1 (0x523030)", Table(PartyAction14_Mode1States), 3); }
extern "C" void __cdecl PartyAction14_Form2(void) { ByState2("PartyAction14_Form2 (0x5230B0)", Table(PartyAction14_Form2States), 3); }

extern "C" void __cdecl PartyFormAction15_ByForm(void) { ByForm("PartyFormAction15_ByForm (0x523970)", Table(PartyFormAction15_Forms)); }
extern "C" void __cdecl PartyAction15_ByForm(void) { ByForm("PartyAction15_ByForm (0x523990)", Table(PartyAction15_Forms)); }
extern "C" void __cdecl PartyFormAction15_Form0(void) { ByState2("PartyFormAction15_Form0 (0x5234F0)", Table(PartyFormAction15_Form0States), 3); }
extern "C" void __cdecl PartyFormAction15_Form1(void) { ByState2("PartyFormAction15_Form1 (0x523510)", Table(PartyFormAction15_Form1States), 3); }
extern "C" void __cdecl PartyFormAction15_Form2(void) { ByState2("PartyFormAction15_Form2 (0x523930)", Table(PartyFormAction15_Form2States), 3); }
extern "C" void __cdecl PartyAction15_Form1(void) { ByState2("PartyAction15_Form1 (0x523530)", Table(PartyAction15_Form1States), 3); }
extern "C" void __cdecl PartyAction15_Form2(void) { ByState2("PartyAction15_Form2 (0x523950)", Table(PartyAction15_Form2States), 2); }

extern "C" void __cdecl PartyFormAction16_Form0(void) { ByState2("PartyFormAction16_Form0 (0x5239B0)", Table(PartyFormAction16_Form0States), 3); }
extern "C" void __cdecl PartyAction16_Form0(void) { ByState2("PartyAction16_Form0 (0x5239D0)", Table(PartyAction16_Form0States), 3); }
extern "C" void __cdecl PartyFormAction16_Form1(void) { ByState2("PartyFormAction16_Form1 (0x523AE0)", Table(PartyFormAction16_Form1States), 3); }
extern "C" void __cdecl PartyAction16_Form1(void) { ByState2("PartyAction16_Form1 (0x523B00)", Table(PartyAction16_Form1Modes), 2); }
extern "C" void __cdecl PartyAction16_Form1Mode0(void) { ByState3("PartyAction16_Form1Mode0 (0x523B20)", Table(PartyAction16_Mode0States), 5); }
extern "C" void __cdecl PartyAction16_Form1Mode1(void) { ByState3("PartyAction16_Form1Mode1 (0x523EB0)", Table(PartyAction16_Mode1States), 3); }

// ===========================================================================
// The states (each the shape named in its body's comment)
// ===========================================================================

// originals 0x522760, 0x5230D0, 0x523550 (0x1D4 bytes each).
extern "C" void __cdecl PartyAction13_Form2Begin(void) { Form2Begin(); }
extern "C" void __cdecl PartyAction14_Form2Begin(void) { Form2Begin(); }
extern "C" void __cdecl PartyAction15_Form1Begin(void) { Form2Begin(); }

// originals 0x522940, 0x5232B0, 0x523730 (0xDB bytes each).
extern "C" void __cdecl PartyAction13_Form2Resolve(void) {
    Form2Resolve("PartyAction13_Form2Resolve (0x522940)", &PartyAction13_CellPickup);
}
extern "C" void __cdecl PartyAction14_Form2Resolve(void) {
    Form2Resolve("PartyAction14_Form2Resolve (0x5232B0)", &PartyAction14_CellPickup);
}
extern "C" void __cdecl PartyAction15_Form1Resolve(void) {
    Form2Resolve("PartyAction15_Form1Resolve (0x523730)", &PartyAction15_CellPickup);
}

// originals 0x522A20, 0x523390, 0x523810 (0x11F bytes each). The original also
// keeps the amount in the low byte of its own z argument's slot (the caller's
// stack, which no caller reads again).
extern "C" unsigned char __cdecl PartyAction13_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }
extern "C" unsigned char __cdecl PartyAction14_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }
extern "C" unsigned char __cdecl PartyAction15_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }

// originals 0x522C00, 0x523B40 (0x87 bytes each).
extern "C" void __cdecl PartyAction14_StrikeBegin(void) { StrikeBegin(); }
extern "C" void __cdecl PartyAction16_StrikeBegin(void) { StrikeBegin(); }

// original 0x523050 (0x35 bytes).
extern "C" void __cdecl PartyAction14_Mode1Begin(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xA] = 0xB;
    S()[3] = 1;
}

// originals 0x522C90, 0x523BD0 (0x14C bytes each).
extern "C" void __cdecl PartyAction14_Strike(void) { Strike("PartyAction14_Strike (0x522C90)", &PartyAction14_StrikeCell); }
extern "C" void __cdecl PartyAction16_Strike(void) { Strike("PartyAction16_Strike (0x523BD0)", &PartyAction16_StrikeCell); }

// originals 0x522E20, 0x523D20 (0x188 bytes each). As CellPickup, the original
// stores the cell byte into the low byte of its z argument's slot.
extern "C" unsigned char __cdecl PartyAction14_StrikeCell(unsigned x, unsigned z) { return StrikeCell(x, z); }
extern "C" unsigned char __cdecl PartyAction16_StrikeCell(unsigned x, unsigned z) { return StrikeCell(x, z); }

void Rest1E_Inject() {
    if (bof3::WantsShadow("rest_1e")) rest_1e::SelfTest();
    BOF3_INJECT(PartyAction_NoAction);
    BOF3_INJECT(PartyFormAction13_Form1);
    BOF3_INJECT(PartyAction13_Form1);
    BOF3_INJECT(PartyFormAction13_Form2);
    BOF3_INJECT(PartyAction13_Form2);
    BOF3_INJECT(PartyAction13_Form2Begin);
    BOF3_INJECT(PartyAction13_Form2Resolve);
    BOF3_INJECT(PartyAction13_CellPickup);
    BOF3_INJECT(PartyFormAction13_ByForm);
    BOF3_INJECT(PartyAction13_ByForm);
    BOF3_INJECT(PartyFormAction14_Form0);
    BOF3_INJECT(PartyFormAction14_Form1);
    BOF3_INJECT(PartyAction14_Form1);
    BOF3_INJECT(PartyAction14_Form1Mode0);
    BOF3_INJECT(PartyAction14_StrikeBegin);
    BOF3_INJECT(PartyAction14_Strike);
    BOF3_INJECT(PartyAction_StrikeWait);
    BOF3_INJECT(PartyAction14_StrikeCell);
    BOF3_INJECT(PartyAction14_Form1Mode1);
    BOF3_INJECT(PartyAction14_Mode1Begin);
    BOF3_INJECT(PartyFormAction14_Form2);
    BOF3_INJECT(PartyAction14_Form2);
    BOF3_INJECT(PartyAction14_Form2Begin);
    BOF3_INJECT(PartyAction14_Form2Resolve);
    BOF3_INJECT(PartyAction14_CellPickup);
    BOF3_INJECT(PartyFormAction14_ByForm);
    BOF3_INJECT(PartyAction14_ByForm);
    BOF3_INJECT(PartyFormAction15_Form0);
    BOF3_INJECT(PartyFormAction15_Form1);
    BOF3_INJECT(PartyAction15_Form1);
    BOF3_INJECT(PartyAction15_Form1Begin);
    BOF3_INJECT(PartyAction15_Form1Resolve);
    BOF3_INJECT(PartyAction15_CellPickup);
    BOF3_INJECT(PartyFormAction15_Form2);
    BOF3_INJECT(PartyAction15_Form2);
    BOF3_INJECT(PartyFormAction15_ByForm);
    BOF3_INJECT(PartyAction15_ByForm);
    BOF3_INJECT(PartyFormAction16_Form0);
    BOF3_INJECT(PartyAction16_Form0);
    BOF3_INJECT(PartyAction_ProbeBegin);
    BOF3_INJECT(PartyFormAction16_Form1);
    BOF3_INJECT(PartyAction16_Form1);
    BOF3_INJECT(PartyAction16_Form1Mode0);
    BOF3_INJECT(PartyAction16_StrikeBegin);
    BOF3_INJECT(PartyAction16_Strike);
    BOF3_INJECT(PartyAction16_StrikeCell);
    BOF3_INJECT(PartyAction16_Form1Mode1);
}
