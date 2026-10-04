// Party sets 2..6's field actions - round fourteen, wave-one group R1B, the 47
// functions 0x51D710..0x51F202 of the cut (analysis/round14_cut.tsv). Each read
// with capstone to its last instruction (docs/rest_1b.md section 1): 29
// dispatchers through .data tables, 14 states, 4 cell handlers. Seven of the
// states and cell handlers are instruction-for-instruction copies of code
// already ours (PartyAction5_Form0Begin, PartyAction5_Form0Resolve,
// Field_CellPickup in field_hidden.cpp; compared with capstone: only the call
// targets differ), kept as their own functions because the tables and calls
// reach each copy by its own address.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; every
// .data table is read in place. No divergence: each is a faithful replacement.
// Where the original would index past a table (an object record by an answer
// no callee gives, a table entry that is not code), ours aborts with a message
// (section 6).
#include "game/rest_1b.h"

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
using move_script::Word;

constexpr unsigned kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr unsigned kObjectStride = 0xA4;   // Sprite_Objects and Sprite_ObjectsExtra
constexpr unsigned kObjects = 0x1E;        // Sprite_Objects' 30, then Sprite_ObjectsExtra's 4
constexpr unsigned kObjectsAll = 0x22;
constexpr U kSteps = 0x6697B0;   // Field_DirectionSteps: 8 rows of two longs
constexpr U kScriptFlagsHigh = 0x9039A3;                // Field_ScriptFlags' high byte (0x9039A2, u16)

unsigned char* S() { return Sprite_Current; }
// A .data table's address (its symbols.toml name is a pointer).
U TableAt(const unsigned long* table) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(table)); }

// A .data dispatch table read in place: the index unchecked, as the original's;
// where the entry is not code (past the run of code-pointer tables) the
// original jumps into data - ours aborts. While the fuzz runs, the entries are
// its recorders (outside .text).
using Handler = void (__cdecl*)();
Handler CodeAt(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const auto entry = static_cast<U>(Long(At(cell)));
    if (!scenario_harness::g_active && (entry < 0x401000 || entry >= 0x5C3000))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    static_cast<unsigned>(entry), static_cast<unsigned>(cell));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}
void ByForm(U table, const char* who) { CodeAt(table, Word(S() + 0x2C), who)(); }
void ByState(U table, const char* who) { CodeAt(table, S()[2], who)(); }
void ByStep(U table, const char* who) { CodeAt(table, S()[3], who)(); }

// Field_DirectionSteps' row for a direction, read in place with the direction
// unmasked (`shl eax, 3` on the zero-extended byte): a byte above 7 reads the
// .data after the table, as the originals do.
U StepX(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8))); }
U StepZ(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8 + 4))); }

// The animation offset of a direction as the originals compute it: (d - 1) / 2
// as a signed division (cdq, sub, sar), so direction 0 gives 0.
unsigned char HalfTurn(unsigned char d) { return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2); }

// The sound of the leader's form: u16 +0x2C + 0x100.
void FormSound(const unsigned char* s) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(s + 0x2C) + 0x100)); }

// An Effect_Objects record by an index the original uses unchecked: ours aborts
// past the 20 (no callee hands such an index: Effect_FindFree, Field_EffectAhead
// and PartyAction_Kind30Ahead answer 0..19 or 0xFF, tested first).
unsigned char* EffectAt(unsigned index, const char* who) {
    if (index >= kEffectCount)
        bof3::Fatal("%s: effect object index 0x%X is past Effect_Objects' 20 records; the original writes 0x%X unchecked", who,
                    index, static_cast<unsigned>(0x7E11E0 + index * kEffectStride));
    return Effect_Objects + index * kEffectStride;
}

// Bit 0 of +0x80 of the object Sprite_ObjectAt found: below 0x1E Sprite_Objects,
// else Sprite_ObjectsExtra (index - 0x1E). `is_signed`: the original compares
// the byte signed (movsx; jge), so 0x80..0xFE index before Sprite_Objects;
// else unsigned (jae), so 0x22..0xFE index past Sprite_ObjectsExtra. Sprite_ObjectAt
// answers 0..0x21 or 0xFF, so neither is reached: ours aborts there.
void MarkObject(unsigned char found, bool is_signed, const char* who) {
    const int i = is_signed ? static_cast<signed char>(found) : static_cast<int>(found);
    if (i < 0 || i >= static_cast<int>(kObjectsAll))
        bof3::Fatal("%s: Sprite_ObjectAt's answer 0x%X is past the 34 objects; the original marks it unchecked", who,
                    static_cast<unsigned>(found));
    if (i < static_cast<int>(kObjects)) Sprite_Objects[i * kObjectStride + 0x80] |= 1;
    else Sprite_ObjectsExtra[(i - kObjects) * kObjectStride + 0x80] |= 1;
}

// An even direction +8 turned one eighth back (-1, & 7); when `probe` answers
// 0, two on (+2), and when it answers 0 again, back to the first turn (-2).
// Sprite_Current re-read after each call; an odd direction is left alone.
template <typename P> void TurnUntil(P probe) {
    unsigned char* const s = S();
    const unsigned char d = s[8];
    if (d & 1) return;
    s[8] = static_cast<unsigned char>((d - 1) & 7);
    if (probe() != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] + 2) & 7);
    if (probe() != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] - 2) & 7);
}

// PartyAction5_Form0Begin's side probe (inline in the copies): the point one
// step in direction d (3 or 5; rows read by address, the direction pushed as
// an immediate); a steep slope there (the scratch flag set, the low word above
// 0x40 signed) and the ground above the sprite's height word: +0x2B = 0.
void ProbeSide(unsigned d) {
    const unsigned char* s = S();
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (At(bof3::addr::DamageScratch)[0] == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = S();
    if (static_cast<short>(Word(s + 0x3E)) < static_cast<short>(ground)) S()[0x2B] = 0;
}

// 0x51D790 / 0x51DF10 (PartyAction5_Form0Begin's code).
void Begin() {
    TurnUntil([] { return SH_CALL(PartyAction_TargetAhead)(); });
    const unsigned char* const s = S();
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (At(bof3::addr::DamageScratch)[0] != 0 && static_cast<short>(slope) > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x46));
        ++S()[2];
    } else {
        S()[0x2B] = 1;
        ProbeSide(3);
        ProbeSide(5);
        FormSound(S());
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
        S()[0xA] = 5;
    }
    S()[0xB] = 0;
    ++S()[2];
}

// The point two steps ahead of Sprite_Current (read now) and its cells.
struct Ahead {
    U x, z;
};
Ahead TwoAhead() {
    const unsigned char* const s = S();
    const unsigned d = s[8];
    return {static_cast<U>(Long(s + 0x34)) + 2u * StepX(d), static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d)};
}

// The cell handler on the point's cell; when it answers 0, on the cell one on
// in x (only when x has a fraction), and when that answers 0 too (or was not
// asked), on the cell one on in z (only when z has one). The cells are the
// point's high words; the original passes dwords whose upper halves are its
// own stack, and every handler reads the low words.
template <typename C> void Cells(const Ahead& p, C cell) {
    const unsigned cx = p.x >> 16, cz = p.z >> 16;
    if (cell(cx, cz) != 0) return;
    if ((p.x & 0xFFFF) != 0 && cell(cx + 1, cz) != 0) return;
    if ((p.z & 0xFFFF) != 0) cell(cx, cz + 1);
}

// 0x51D950 / 0x51E0D0 (PartyAction5_Form0Resolve's code).
template <typename C> void Resolve(C cell, const char* who) {
    --S()[0xA];
    if (S()[0xA] == 0) {
        const Ahead p = TwoAhead();
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(p.x), static_cast<long>(p.z), 0);
        if (found != 0xFF) MarkObject(found, true, who);
        Cells(p, cell);
        ++S()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51DAA0 / 0x51E1B0 (Field_CellPickup's code): 0xF2 - with an effect object
// free, Effect_SpawnAtCell(0), three Rand draws in sixteen find zenny (2, or 5
// on 15; ten times that when Field_InputFlags has bit 1 or 2 and a second Rand
// & 3 is 0) and Effect_SpawnAtCell(1), +0xB = 1; 0xF8 - Effect_SpawnAtCell(0),
// item 0x56's name into Text_Records, Inventory_Add: taken, a sound and message
// 2, else message 3; Field_Request = 2, +0xB = 1. Both clear the cell; 1.
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
            S()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) move_script::SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x56, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
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

// 0x51DC00 / 0x51E310.
void Form1Begin(const char* who) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Field_State[0x137] = 0;
            return;
        }
        const unsigned char* const s = S();
        EffectAt(k, who)[0xB] = 1;
        FormSound(s);
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(S()[8] + 8));
        Field_State[0x128] = 2;
        At(kScriptFlagsHigh)[0] |= 0x10;
        SH_CALL(Field_JumpStart)();
        --S()[9];
        SH_CALL(Field_LeaderStepTick)();
        Field_State[0x137] = 1;
        ++S()[2];
        return;
    }
    const Ahead p = TwoAhead();
    if ((Field_State[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(p.x), static_cast<long>(p.z), 1);
        if (found != 0xFF) MarkObject(found, false, who);
    }
    Field_State[0x137] = 0;
}

// 0x51E570 / 0x51EDF0. `sounds_first`: set 4's makes the form's sound before
// Field_EffectAhead and its 0x10B before it marks the effect object; set 5's
// neither first.
template <typename C> void Hit(C cell, bool sounds_first, const char* who) {
    unsigned char* s = S();
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        s = S();
        if (s[0xA] == 0) {
            if (sounds_first) FormSound(s);
            const unsigned char e = SH_CALL(Field_EffectAhead)();
            if (e != 0xFF) {
                unsigned char* const object = EffectAt(e, who);
                if (sounds_first) {
                    SH_CALL(Sound_PlayEffect)(0x10B);
                    unsigned char* const t = S();
                    object[8] = t[8];
                    object[0xA] = 1;
                    t[6] = e;
                } else {
                    object[8] = S()[8];
                    object[0xA] = 1;
                    SH_CALL(Sound_PlayEffect)(0x10B);
                    S()[6] = e;
                }
                ++S()[3];
            } else {
                const Ahead p = TwoAhead();
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(p.x), static_cast<long>(p.z), 0);
                if (found != 0xFF) {
                    MarkObject(found, true, who);
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                Cells(p, cell);
            }
            ++S()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51E6C0 / 0x51EF30.
unsigned char CellHit(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if ((static_cast<unsigned>(SH_CALL(Rand)()) & 7u) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
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
        for (unsigned k = 0; k < 16; k += 4) move_script::SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        S()[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Field_State[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        S()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

}  // namespace

// ===========================================================================
// The dispatchers (section 1.1): `mov ecx, [Sprite_Current]; xor eax, eax;
// mov ax / al, ...; jmp [eax * 4 + table]`, the index unchecked.
// ===========================================================================

#define R1B_TABLE(t) TableAt(t)
extern "C" void __cdecl PartyFormAction2_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction2_Forms), "PartyFormAction2_ByForm (0x51D710)"); }
extern "C" void __cdecl PartyAction2_ByForm(void) { ByForm(R1B_TABLE(PartyAction2_Forms), "PartyAction2_ByForm (0x51D730)"); }
extern "C" void __cdecl PartyFormAction3_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction3_Forms), "PartyFormAction3_ByForm (0x51DE90)"); }
extern "C" void __cdecl PartyAction3_ByForm(void) { ByForm(R1B_TABLE(PartyAction3_Forms), "PartyAction3_ByForm (0x51DEB0)"); }
extern "C" void __cdecl PartyFormAction4_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction4_Forms), "PartyFormAction4_ByForm (0x51E8B0)"); }
extern "C" void __cdecl PartyAction4_ByForm(void) { ByForm(R1B_TABLE(PartyAction4_Forms), "PartyAction4_ByForm (0x51E8D0)"); }
extern "C" void __cdecl PartyFormAction5_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction5_Forms), "PartyFormAction5_ByForm (0x51F190)"); }

extern "C" void __cdecl PartyFormAction3_Form0(void) { ByState(R1B_TABLE(PartyFormAction3_Form0States), "PartyFormAction3_Form0 (0x51D750)"); }
extern "C" void __cdecl PartyAction3_Form0(void) { ByState(R1B_TABLE(PartyAction3_Form0States), "PartyAction3_Form0 (0x51D770)"); }
extern "C" void __cdecl PartyFormAction3_Form1(void) { ByState(R1B_TABLE(PartyFormAction3_Form1States), "PartyFormAction3_Form1 (0x51DBC0)"); }
extern "C" void __cdecl PartyAction3_Form1(void) { ByState(R1B_TABLE(PartyAction3_Form1States), "PartyAction3_Form1 (0x51DBE0)"); }
extern "C" void __cdecl PartyFormAction3_Form2(void) { ByState(R1B_TABLE(PartyFormAction3_Form2States), "PartyFormAction3_Form2 (0x51DDE0)"); }
extern "C" void __cdecl PartyAction3_Form2(void) { ByState(R1B_TABLE(PartyAction3_Form2States), "PartyAction3_Form2 (0x51DE00)"); }
extern "C" void __cdecl PartyFormAction4_Form0(void) { ByState(R1B_TABLE(PartyFormAction4_Form0States), "PartyFormAction4_Form0 (0x51DED0)"); }
extern "C" void __cdecl PartyAction4_Form0(void) { ByState(R1B_TABLE(PartyAction4_Form0States), "PartyAction4_Form0 (0x51DEF0)"); }
extern "C" void __cdecl PartyFormAction4_Form1(void) { ByState(R1B_TABLE(PartyFormAction4_Form1States), "PartyFormAction4_Form1 (0x51E2D0)"); }
extern "C" void __cdecl PartyAction4_Form1(void) { ByState(R1B_TABLE(PartyAction4_Form1States), "PartyAction4_Form1 (0x51E2F0)"); }
extern "C" void __cdecl PartyFormAction4_Form2(void) { ByState(R1B_TABLE(PartyFormAction4_Form2States), "PartyFormAction4_Form2 (0x51E480)"); }
extern "C" void __cdecl PartyAction4_Form2(void) { ByState(R1B_TABLE(PartyAction4_Form2States), "PartyAction4_Form2 (0x51E4A0)"); }
extern "C" void __cdecl PartyFormAction5_Form0(void) { ByState(R1B_TABLE(PartyFormAction5_Form0States), "PartyFormAction5_Form0 (0x51E8F0)"); }
extern "C" void __cdecl PartyFormAction5_Form1(void) { ByState(R1B_TABLE(PartyFormAction5_Form1States), "PartyFormAction5_Form1 (0x51ECF0)"); }
extern "C" void __cdecl PartyAction5_Form1(void) { ByState(R1B_TABLE(PartyAction5_Form1States), "PartyAction5_Form1 (0x51ED10)"); }
extern "C" void __cdecl PartyFormAction5_Form2(void) { ByState(R1B_TABLE(PartyFormAction5_Form2States), "PartyFormAction5_Form2 (0x51F170)"); }
extern "C" void __cdecl PartyFormAction6_Form0(void) { ByState(R1B_TABLE(PartyFormAction6_Form0States), "PartyFormAction6_Form0 (0x51F1D0)"); }
extern "C" void __cdecl PartyAction6_Form0(void) { ByState(R1B_TABLE(PartyAction6_Form0States), "PartyAction6_Form0 (0x51F1F0)"); }

extern "C" void __cdecl PartyAction4_Form2State0(void) { ByStep(R1B_TABLE(PartyAction4_Form2State0Steps), "PartyAction4_Form2State0 (0x51E4C0)"); }
extern "C" void __cdecl PartyAction4_Form2State1(void) { ByStep(R1B_TABLE(PartyAction4_Form2State1Steps), "PartyAction4_Form2State1 (0x51E850)"); }
extern "C" void __cdecl PartyAction5_Form1State0(void) { ByStep(R1B_TABLE(PartyAction5_Form1State0Steps), "PartyAction5_Form1State0 (0x51ED30)"); }
extern "C" void __cdecl PartyAction5_Form1State1(void) { ByStep(R1B_TABLE(PartyAction5_Form1State1Steps), "PartyAction5_Form1State1 (0x51F0C0)"); }
#undef R1B_TABLE

// ===========================================================================
// The states and the cell handlers (sections 1.2..1.5)
// ===========================================================================

// original 0x51D790 (0x1B1 bytes), set 3's form 0, state 0.
extern "C" void __cdecl PartyAction3_Form0Begin(void) { Begin(); }
// original 0x51DF10 (0x1B1 bytes), set 4's form 0, state 0.
extern "C" void __cdecl PartyAction4_Form0Begin(void) { Begin(); }

// original 0x51D950 (0xDB bytes), set 3's form 0, state 1: its cell handler 0x51DAA0.
extern "C" void __cdecl PartyAction3_Form0Resolve(void) {
    Resolve([](unsigned x, unsigned z) { return SH_CALL(PartyAction3_CellPickup)(x, z); }, "PartyAction3_Form0Resolve (0x51D950)");
}
// original 0x51E0D0 (0xDB bytes), set 4's form 0, state 1: its cell handler 0x51E1B0.
extern "C" void __cdecl PartyAction4_Form0Resolve(void) {
    Resolve([](unsigned x, unsigned z) { return SH_CALL(PartyAction4_CellPickup)(x, z); }, "PartyAction4_Form0Resolve (0x51E0D0)");
}

// original 0x51DAA0 (0x11F bytes) / 0x51E1B0 (0x11F bytes).
extern "C" unsigned char __cdecl PartyAction3_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }
extern "C" unsigned char __cdecl PartyAction4_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }

// original 0x51DC00 (0x166 bytes), set 3's form 1, state 0; 0x51E310 (0x166
// bytes), set 4's. PartyAction_Kind30Ahead's index (0..19, or 0xFF) handed to
// PartyAction_MemberOnEffect and PartyAction_MemberBeyondEffect (the dword the
// original pushes holds it in its low byte over its own ecx).
extern "C" void __cdecl PartyAction3_Form1Begin(void) { Form1Begin("PartyAction3_Form1Begin (0x51DC00)"); }
extern "C" void __cdecl PartyAction4_Form1Begin(void) { Form1Begin("PartyAction4_Form1Begin (0x51E310)"); }

// original 0x51DE20 (0x6E bytes): Sprite_ScriptTick; unless the script position
// word +0x58 is then 0xA, nothing more. Else +0xB = Effect_FindFree, read back
// from the record; not 0xFF: that effect object's +0 = 1 and kind +5 = 0x3A
// (+0xB read again for each). The form's sound either way, then +7 = 0 and +2
// one on, Sprite_Current re-read after the sound.
extern "C" void __cdecl PartyAction_SpawnKind3A(void) {
    SH_CALL(Sprite_ScriptTick)();
    if (Word(S() + 0x58) != 0xA) return;
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    S()[0xB] = slot;
    unsigned char* const s = S();
    if (s[0xB] != 0xFF) {
        EffectAt(s[0xB], "PartyAction_SpawnKind3A (0x51DE20)")[0] = 1;
        EffectAt(s[0xB], "PartyAction_SpawnKind3A (0x51DE20)")[5] = 0x3A;
    }
    FormSound(s);
    S()[7] = 0;
    ++S()[2];
}

// original 0x51E4E0 (0x87 bytes), set 4's form 2, state 0 step 0: an even
// direction turned toward a blocked cell (PartyAction_BlockedAhead answering 1
// keeps the turn); PartyAction_SideProbes; the pose (+8 - 1) / 2 + 0x42; +0xB
// = 0, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction4_Form2Aim(void) {
    TurnUntil([] { return SH_CALL(PartyAction_BlockedAhead)(); });
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xB] = 0;
    S()[0xA] = 0xB;
    S()[3] = 1;
}

// original 0x51ED50 (0xA0 bytes), set 5's form 1, state 0 step 0: the same
// turn; the form's sound; the pose (+8 - 1) / 2 + 0x42; +0xB = 0, +6 = 0, +0xA
// = 8, +3 = 1.
extern "C" void __cdecl PartyAction5_Form1Aim(void) {
    TurnUntil([] { return SH_CALL(PartyAction_BlockedAhead)(); });
    FormSound(S());
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xB] = 0;
    S()[6] = 0;
    S()[0xA] = 8;
    S()[3] = 1;
}

// original 0x51E570 (0x14C bytes), set 4's form 2, state 0 step 1: its cell handler 0x51E6C0.
extern "C" void __cdecl PartyAction4_Form2Hit(void) {
    Hit([](unsigned x, unsigned z) { return SH_CALL(PartyAction4_CellHit)(x, z); }, true, "PartyAction4_Form2Hit (0x51E570)");
}
// original 0x51EDF0 (0x140 bytes), set 5's form 1, state 0 step 1: its cell handler 0x51EF30.
extern "C" void __cdecl PartyAction5_Form1Hit(void) {
    Hit([](unsigned x, unsigned z) { return SH_CALL(PartyAction5_CellHit)(x, z); }, false, "PartyAction5_Form1Hit (0x51EDF0)");
}

// original 0x51E6C0 (0x188 bytes) / 0x51EF30 (0x188 bytes). The original also
// stores the cell's code into the low byte of its own z argument slot and
// pushes that dword as a fourth argument Effect_SpawnAtCellHigh never reads.
extern "C" unsigned char __cdecl PartyAction4_CellHit(unsigned x, unsigned z) { return CellHit(x, z); }
extern "C" unsigned char __cdecl PartyAction5_CellHit(unsigned x, unsigned z) { return CellHit(x, z); }

// original 0x51E870 (0x35 bytes), set 4's form 2, state 1 step 0:
// PartyAction_SideProbes, the pose (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction4_Form2Again(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xA] = 0xB;
    S()[3] = 1;
}

// original 0x51F0E0 (0x46 bytes), set 5's form 1, state 1 step 0 (and a cell
// of another set's table, 0x65FE88): the form's sound, the pose (+8 - 1) / 2 +
// 0x42, +0xA = 8, +3 = 1.
extern "C" void __cdecl PartyAction5_Form1Again(void) {
    FormSound(S());
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xA] = 8;
    S()[3] = 1;
}

// original 0x51F130 (0x3C bytes), set 5's form 1, state 1 step 2 (and
// 0x65FE90): the effect object +0xB names free (+0) or in state 1 (+1), and
// Field_Kind2Hold 0: Field_State +0x137 = 0. Then Sprite_ScriptTick (a tail
// jump in the original; its answer is no caller's).
extern "C" void __cdecl PartyAction5_Form1Wait(void) {
    const unsigned char* const e = EffectAt(S()[0xB], "PartyAction5_Form1Wait (0x51F130)");
    if ((e[0] == 0 || e[1] == 1) && Field_Kind2Hold == 0) Field_State[0x137] = 0;
    SH_CALL(Sprite_ScriptTick)();
}

void Rest1B_Inject() {
    if (bof3::WantsShadow("rest_1b")) rest_1b::SelfTest();
    BOF3_INJECT(PartyFormAction2_ByForm);
    BOF3_INJECT(PartyAction2_ByForm);
    BOF3_INJECT(PartyFormAction3_Form0);
    BOF3_INJECT(PartyAction3_Form0);
    BOF3_INJECT(PartyAction3_Form0Begin);
    BOF3_INJECT(PartyAction3_Form0Resolve);
    BOF3_INJECT(PartyAction3_CellPickup);
    BOF3_INJECT(PartyFormAction3_Form1);
    BOF3_INJECT(PartyAction3_Form1);
    BOF3_INJECT(PartyAction3_Form1Begin);
    BOF3_INJECT(PartyFormAction3_Form2);
    BOF3_INJECT(PartyAction3_Form2);
    BOF3_INJECT(PartyAction_SpawnKind3A);
    BOF3_INJECT(PartyFormAction3_ByForm);
    BOF3_INJECT(PartyAction3_ByForm);
    BOF3_INJECT(PartyFormAction4_Form0);
    BOF3_INJECT(PartyAction4_Form0);
    BOF3_INJECT(PartyAction4_Form0Begin);
    BOF3_INJECT(PartyAction4_Form0Resolve);
    BOF3_INJECT(PartyAction4_CellPickup);
    BOF3_INJECT(PartyFormAction4_Form1);
    BOF3_INJECT(PartyAction4_Form1);
    BOF3_INJECT(PartyAction4_Form1Begin);
    BOF3_INJECT(PartyFormAction4_Form2);
    BOF3_INJECT(PartyAction4_Form2);
    BOF3_INJECT(PartyAction4_Form2State0);
    BOF3_INJECT(PartyAction4_Form2Aim);
    BOF3_INJECT(PartyAction4_Form2Hit);
    BOF3_INJECT(PartyAction4_CellHit);
    BOF3_INJECT(PartyAction4_Form2State1);
    BOF3_INJECT(PartyAction4_Form2Again);
    BOF3_INJECT(PartyFormAction4_ByForm);
    BOF3_INJECT(PartyAction4_ByForm);
    BOF3_INJECT(PartyFormAction5_Form0);
    BOF3_INJECT(PartyFormAction5_Form1);
    BOF3_INJECT(PartyAction5_Form1);
    BOF3_INJECT(PartyAction5_Form1State0);
    BOF3_INJECT(PartyAction5_Form1Aim);
    BOF3_INJECT(PartyAction5_Form1Hit);
    BOF3_INJECT(PartyAction5_CellHit);
    BOF3_INJECT(PartyAction5_Form1State1);
    BOF3_INJECT(PartyAction5_Form1Again);
    BOF3_INJECT(PartyAction5_Form1Wait);
    BOF3_INJECT(PartyFormAction5_Form2);
    BOF3_INJECT(PartyFormAction5_ByForm);
    BOF3_INJECT(PartyFormAction6_Form0);
    BOF3_INJECT(PartyAction6_Form0);
}
