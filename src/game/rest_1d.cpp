// Party sets 9 to 12's field actions - round fourteen, wave one, group R1D:
// the 46 functions 0x520E10..0x5226C1 of the cut (analysis/round14_cut.tsv),
// each read with capstone to its last instruction (docs/rest_1d.md section 1).
// 27 are dispatchers jumping through a .data state table; 19 are state
// handlers and the two cell probes they call. Three bodies recur unchanged in
// the band (the side-or-slope start, the resolve and the cell pickup, once per
// set 9, 10 and 11) and one twice (set 10's and 11's kind-0x30 start): each
// copy is its own function, every copy compared against its own original.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; a table's
// handler is called by the address the table holds. No divergence: each is a
// faithful replacement. Where the original indexes a table past its own count
// with a byte no handler writes, or a record array by an answer no callee
// gives, ours aborts with a message (section 5); the one index play reaches
// (PartyAction_WaitEffectDone's +0xB of 0xFF) is read in place as the
// original reads it.
#include "game/rest_1d.h"

#include <cstdint>
#include <cstdlib>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1d_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_1d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::Word;

constexpr unsigned kSpriteStride = 0xA4;
constexpr unsigned kEffectStride = 0x80;
constexpr U kEffects = 0x7E11E0;   // Effect_Objects, 20 records

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_1d.md section 3). Past it the original jumps through the
// next table's cell; no handler writes such an index (section 5), so ours
// aborts. The entry is called as the cell holds it: Capcom's address in the
// game (Inject's jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();
void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(table), static_cast<unsigned>(table + 4 * index));
    const auto entry = static_cast<U>(Long(At(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry))();
}

// Field_DirectionSteps' row for a direction, read in place with the byte
// unmasked (`shl eax, 3` on the zero-extended byte), as every reader of it in
// the originals does (docs/rest_0a.md section 5).
U StepX(unsigned d) { return static_cast<U>(Long(At(at::kSteps + d * 8))); }
U StepZ(unsigned d) { return static_cast<U>(Long(At(at::kSteps + d * 8 + 4))); }

// The pose offset of a direction as the originals compute it: (d - 1) / 2 as a
// signed division (cdq, sub, sar), so direction 0 gives 0; then `add al, k`.
unsigned char Pose(unsigned char d, unsigned k) {
    return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2 + static_cast<int>(k));
}

std::uint16_t SoundOfForm() { return static_cast<std::uint16_t>(Word(Sprite_Current + 0x2C) + 0x100); }

// An effect record by an index the original takes unchecked from a callee's
// answer or a sprite byte that no path in play makes 20 or more (section 5):
// ours aborts there.
unsigned char* EffectChecked(unsigned index, const char* who) {
    if (index >= 20)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original writes at 0x%X", who, index,
                    static_cast<unsigned>(kEffects + index * kEffectStride));
    return Effect_Objects + index * kEffectStride;
}

// Sprite_ObjectAt's object marked: +0x80 |= 1 of Sprite_Objects' record below
// 0x1E, else of Sprite_ObjectsExtra's (index - 0x1E). `is_signed`: the
// original compares the byte signed (movsx, jge), else unsigned (jae). Either
// way the answer is 0..0x21 (the 30 and the 4 records); ours aborts on any
// other, where the original would write before Sprite_Objects (signed) or past
// Sprite_ObjectsExtra's four.
void MarkObject(unsigned char found, bool is_signed, const char* who) {
    if (found > 0x21)
        bof3::Fatal("%s: Sprite_ObjectAt answered %u, past the 34 sprite records; the original marks it unchecked (%s)", who, found,
                    is_signed ? "signed" : "unsigned");
    if (found < 0x1E) At(at::kSpriteFlag + found * kSpriteStride)[0] |= 1;
    else At(at::kExtraFlag + (found - 0x1E) * kSpriteStride)[0] |= 1;
}

// The point two steps ahead of Sprite_Current (x + 2 * step, z + 2 * step).
void TwoAhead(U& x, U& z) {
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
    z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
}

// The cells around a point the resolves try in turn: the point's cell; when
// that answers 0 and x has a fraction, the cell one on in x; when that also
// answers 0 (or x had none) and z has a fraction, the cell one on in z. The
// cells are the point's high words; the originals pass them as dwords whose
// upper halves are uninitialised stack, and every callee reads 16 bits.
using CellProbe = unsigned char (__cdecl*)(unsigned, unsigned);
void TryCells(CellProbe probe, U x, U z) {
    const unsigned cx = x >> 16, cz = z >> 16;
    if (probe(cx, cz) != 0) return;
    if ((x & 0xFFFF) != 0 && probe(cx + 1, cz) != 0) return;
    if ((z & 0xFFFF) != 0) probe(cx, cz + 1);
}

// The inline side probe of 0x520F40's copies, direction 3 or 5: the point one
// step that way (Sprite_Current re-read); the slope there steep (the scratch
// flag set and its low word above 0x40, signed) and the ground there above the
// sprite's height word (both s16): +0x2B = 0. PartyAction_SideProbes' probe.
void SideProbe(unsigned d) {
    const unsigned char* s = Sprite_Current;
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (At(bof3::addr::DamageScratch)[0] == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    if (static_cast<short>(Word(s + 0x3E)) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}

// --- the three bodies sets 9, 10 and 11 share ----------------------------------

// 0x520F40 / 0x521600 / 0x521CC0. An even direction is turned one eighth back;
// when PartyAction_TargetAhead finds nothing that way, two on, and when it
// finds nothing there either, back to the first turn. Then, one step ahead in
// +8 (read after the turns): the ground (MapView_GroundAt) less the height word
// +0x3E (re-read after the call), as a short, and MapView_SlopeAt there with
// the direction re-read after the ground; the scratch flag set and that rise
// above 0x40 - the pose (+8 - 1) / 2 + 0x46 and +2 one on. Otherwise +0x2B = 1,
// the side probes in directions 3 and 5, Sound_PlayEffect(+0x2C + 0x100), the
// pose + 0x42 and +0xA = 5. Then +0xB = 0 and +2 one on. The slope's answer
// itself is not read; the rise is the ground's, unlike PartyAction5_Form0Begin.
void FormBegin() {
    {
        unsigned char* const s = Sprite_Current;
        if ((s[8] & 1) == 0) {
            s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
            if (SH_CALL(PartyAction_TargetAhead)() == 0) {
                Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 2) & 7);
                if (SH_CALL(PartyAction_TargetAhead)() == 0) Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] - 2) & 7);
            }
        }
    }
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(Sprite_Current + 0x3E)));
    const unsigned direction = Sprite_Current[8];
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), direction);
    if (At(bof3::addr::DamageScratch)[0] != 0 && rise > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x46));
        ++Sprite_Current[2];
    } else {
        Sprite_Current[0x2B] = 1;
        SideProbe(3);
        SideProbe(5);
        SH_CALL(Sound_PlayEffect)(SoundOfForm());
        SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x42));
        Sprite_Current[0xA] = 5;
    }
    Sprite_Current[0xB] = 0;
    ++Sprite_Current[2];
}

// 0x521120 / 0x5217E0 / 0x521EA0: PartyAction5_Form0Resolve's code (instruction
// for instruction but the pickup it calls). +0xA one down; at 0: the point two
// steps ahead; the object Sprite_ObjectAt(point, 0) finds gets +0x80 bit 0 (the
// byte compared signed); the cells tried with the set's pickup; +2 one on.
// Sprite_ScriptTickOnce every time.
void FormResolve(CellProbe pickup, const char* who) {
    --Sprite_Current[0xA];
    if (Sprite_Current[0xA] == 0) {
        U x, z;
        TwoAhead(x, z);
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (found != 0xFF) MarkObject(found, true, who);
        TryCells(pickup, x, z);
        ++Sprite_Current[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x521200 / 0x5218C0 / 0x521F80: Field_CellPickup's code with the bonus's
// multiplier 0x14 (imul cl; Field_CellPickup's is 0xA). AreaMap_ByteAt(x, z):
//   0xF2 - with an effect object free (Effect_FindFree): Effect_SpawnAtCell(0,
//          x, z); Rand & 0xF at least 0xD finds zenny, 2 (5 on 0xF), twenty
//          times that when Field_InputFlags (read after the draw) has bit 1 or
//          2 and a second Rand & 3 is 0: Field_GiveZenny, Effect_SpawnAtCell(1,
//          x, z); then +0xB = 1. With no object free, none of that.
//   0xF8 - Effect_SpawnAtCell(0, x, z); item 0x56's name (Item_NamePtr(0,
//          0x56), 16 bytes) into Text_Records; Inventory_Add(0, 0x56, 1): taken,
//          Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (AreaMap_ClearCell) and answer 1; anything else 0.
// x and z go on as the dwords given (every callee reads their low words).
unsigned char CellPickup(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 0x14);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sprite_Current[0xB] = 1;
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
        Sprite_Current[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x5213A0 / 0x521AD0. The kind-0x30 effect object lined up one step ahead
// (PartyAction_Kind30Ahead): when PartyAction_MemberOnEffect and then
// PartyAction_MemberBeyondEffect both answer 0 for it - its +0xB = 1,
// Sound_PlayEffect(+0x2C + 0x100), the pose +8 + 8, Field_State +0x128 = 2,
// Field_ScriptFlags' bit 0x1000 set (its high byte | 0x10), Field_JumpStart,
// +9 one down, Field_LeaderStepTick, Field_State +0x137 = 1 and +2 one on; when
// either answers not 0, Field_State +0x137 = 0. With none lined up: the point
// two steps ahead; unless Field_State +0x138 has bit 0, the object
// Sprite_ObjectAt(point, margin 1) finds gets +0x80 bit 0 (the byte compared
// unsigned); Field_State +0x137 = 0.
void Kind30Begin(const char* who) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Field_State[0x137] = 0;
            return;
        }
        EffectChecked(k, who)[0xB] = 1;
        SH_CALL(Sound_PlayEffect)(SoundOfForm());
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 8));
        Field_State[0x128] = 2;
        At(at::kScriptFlagsHi)[0] |= 0x10;
        SH_CALL(Field_JumpStart)();
        --Sprite_Current[9];
        SH_CALL(Field_LeaderStepTick)();
        Field_State[0x137] = 1;
        ++Sprite_Current[2];
        return;
    }
    U x, z;
    TwoAhead(x, z);
    if ((Field_State[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 1);
        if (found != 0xFF) MarkObject(found, false, who);
    }
    Field_State[0x137] = 0;
}

}  // namespace

// ===========================================================================
// The dispatchers. Each is `mov ecx, [Sprite_Current]; xor eax, eax; mov al /
// ax, [ecx + k]; jmp [eax * 4 + table]` (0x12 / 0x13 bytes): the index zero-
// extended, unchecked; ours aborts past the table's count (Jump).
// ===========================================================================

// Field_ActionBySet's and Field_FormActions' entries for sets 9..12: by the u16
// form word +0x2C.
extern "C" void __cdecl PartyAction9_ByForm(void) { Jump(at::kAction9Forms, 3, Word(Sprite_Current + 0x2C), "PartyAction9_ByForm (0x521340)"); }
extern "C" void __cdecl PartyFormAction9_ByForm(void) {
    Jump(at::kFormAction9Forms, 3, Word(Sprite_Current + 0x2C), "PartyFormAction9_ByForm (0x521320)");
}
extern "C" void __cdecl PartyAction10_ByForm(void) {
    Jump(at::kAction10Forms, 3, Word(Sprite_Current + 0x2C), "PartyAction10_ByForm (0x521A70)");
}
extern "C" void __cdecl PartyFormAction10_ByForm(void) {
    Jump(at::kFormAction10Forms, 3, Word(Sprite_Current + 0x2C), "PartyFormAction10_ByForm (0x521A50)");
}
extern "C" void __cdecl PartyAction11_ByForm(void) {
    Jump(at::kAction11Forms, 3, Word(Sprite_Current + 0x2C), "PartyAction11_ByForm (0x5220C0)");
}
extern "C" void __cdecl PartyFormAction11_ByForm(void) {
    Jump(at::kFormAction11Forms, 3, Word(Sprite_Current + 0x2C), "PartyFormAction11_ByForm (0x5220A0)");
}
extern "C" void __cdecl PartyAction12_ByForm(void) {
    Jump(at::kAction12Forms, 3, Word(Sprite_Current + 0x2C), "PartyAction12_ByForm (0x522690)");
}
extern "C" void __cdecl PartyFormAction12_ByForm(void) {
    Jump(at::kFormAction12Forms, 3, Word(Sprite_Current + 0x2C), "PartyFormAction12_ByForm (0x522670)");
}

// The forms: by the state byte +2.
extern "C" void __cdecl PartyFormAction9_Form2(void) { Jump(at::kFormAction9Form2States, 3, Sprite_Current[2], "PartyFormAction9_Form2 (0x520E70)"); }
extern "C" void __cdecl PartyAction9_Form2(void) { Jump(at::kAction9Form2States, 3, Sprite_Current[2], "PartyAction9_Form2 (0x520F20)"); }
extern "C" void __cdecl PartyFormAction10_Form0(void) { Jump(at::kFormAction10Form0States, 3, Sprite_Current[2], "PartyFormAction10_Form0 (0x521360)"); }
extern "C" void __cdecl PartyAction10_Form0(void) { Jump(at::kAction10Form0States, 2, Sprite_Current[2], "PartyAction10_Form0 (0x521380)"); }
extern "C" void __cdecl PartyFormAction10_Form1(void) { Jump(at::kFormAction10Form1States, 3, Sprite_Current[2], "PartyFormAction10_Form1 (0x5215C0)"); }
extern "C" void __cdecl PartyAction10_Form1(void) { Jump(at::kAction10Form1States, 3, Sprite_Current[2], "PartyAction10_Form1 (0x5215E0)"); }
extern "C" void __cdecl PartyFormAction10_Form2(void) { Jump(at::kFormAction10Form2States, 3, Sprite_Current[2], "PartyFormAction10_Form2 (0x5219E0)"); }
extern "C" void __cdecl PartyAction10_Form2(void) { Jump(at::kAction10Form2States, 2, Sprite_Current[2], "PartyAction10_Form2 (0x521A00)"); }
extern "C" void __cdecl PartyFormAction11_Form0(void) { Jump(at::kFormAction11Form0States, 3, Sprite_Current[2], "PartyFormAction11_Form0 (0x521A90)"); }
extern "C" void __cdecl PartyAction11_Form0(void) { Jump(at::kAction11Form0States, 2, Sprite_Current[2], "PartyAction11_Form0 (0x521AB0)"); }
extern "C" void __cdecl PartyFormAction11_Form1(void) { Jump(at::kFormAction11Form1States, 3, Sprite_Current[2], "PartyFormAction11_Form1 (0x521C80)"); }
extern "C" void __cdecl PartyAction11_Form1(void) { Jump(at::kAction11Form1States, 3, Sprite_Current[2], "PartyAction11_Form1 (0x521CA0)"); }
extern "C" void __cdecl PartyFormAction12_Form0(void) { Jump(at::kFormAction12Form0States, 3, Sprite_Current[2], "PartyFormAction12_Form0 (0x5220E0)"); }
extern "C" void __cdecl PartyAction12_Form0(void) { Jump(at::kAction12Form0States, 2, Sprite_Current[2], "PartyAction12_Form0 (0x522100)"); }
extern "C" void __cdecl PartyFormAction12_Form1(void) { Jump(at::kFormAction12Form1States, 3, Sprite_Current[2], "PartyFormAction12_Form1 (0x522650)"); }
extern "C" void __cdecl PartyFormAction13_Form0(void) { Jump(at::kFormAction13Form0States, 3, Sprite_Current[2], "PartyFormAction13_Form0 (0x5226B0)"); }

// The states with steps: by the byte +3.
extern "C" void __cdecl PartyAction9_Form1State1(void) {
    Jump(at::kAction9Form1State1Steps, 3, Sprite_Current[3], "PartyAction9_Form1State1 (0x520E10)");
}
extern "C" void __cdecl PartyAction12_Form0State0(void) {
    Jump(at::kAction12Form0State0Steps, 5, Sprite_Current[3], "PartyAction12_Form0State0 (0x522120)");
}
extern "C" void __cdecl PartyAction12_Form0State1(void) {
    Jump(at::kAction12Form0State1Steps, 3, Sprite_Current[3], "PartyAction12_Form0State1 (0x5224B0)");
}

// ===========================================================================
// The state handlers
// ===========================================================================

// original 0x520E30 (0x35 bytes), PartyAction9_Form1State1Steps[0]:
// PartyAction_SideProbes, Sprite_EnsureAnimation((+8 - 1) / 2 + 0x42), +0xA =
// 0xB, +3 = 1.
extern "C" void __cdecl PartyAction9_Form1Start(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x42));
    Sprite_Current[0xA] = 0xB;
    Sprite_Current[3] = 1;
}

// original 0x520E90 (0x87 bytes), entry 0 of eleven state tables: Cond_ByteFA
// 0xF - Field_State +0x137 = 0 and nothing else. Otherwise, d = +8: the target
// side is 5 when |d - 3| > |d - 5| (signed, cdq), else 3; +0xB =
// Sprite_TurnSense(target), +3 = target, +9 = 2, +2 one on.
extern "C" void __cdecl PartyFormAction_TurnToSide(void) {
    if (static_cast<unsigned char>(Cond_ByteFA) == 0xF) {
        Field_State[0x137] = 0;
        return;
    }
    const int d = Sprite_Current[8];
    const unsigned char target = std::abs(d - 3) > std::abs(d - 5) ? 5 : 3;
    const unsigned char sense = SH_CALL(Sprite_TurnSense)(target);
    Sprite_Current[0xB] = sense;
    Sprite_Current[3] = target;
    Sprite_Current[9] = 2;
    ++Sprite_Current[2];
}

// originals 0x520F40 / 0x521600 / 0x521CC0 (0x1D4 bytes each, the same
// instructions): FormBegin above.
extern "C" void __cdecl PartyAction9_Form2Begin(void) { FormBegin(); }
extern "C" void __cdecl PartyAction10_Form1Begin(void) { FormBegin(); }
extern "C" void __cdecl PartyAction11_Form1Begin(void) { FormBegin(); }

// originals 0x521120 / 0x5217E0 / 0x521EA0 (0xDB bytes each): FormResolve,
// each calling its own set's pickup (its E8 sites at +0x8F, +0xA5, +0xB9).
extern "C" void __cdecl PartyAction9_Form2Resolve(void) {
    FormResolve(SH_CALL(PartyAction9_CellPickup), "PartyAction9_Form2Resolve (0x521120)");
}
extern "C" void __cdecl PartyAction10_Form1Resolve(void) {
    FormResolve(SH_CALL(PartyAction10_CellPickup), "PartyAction10_Form1Resolve (0x5217E0)");
}
extern "C" void __cdecl PartyAction11_Form1Resolve(void) {
    FormResolve(SH_CALL(PartyAction11_CellPickup), "PartyAction11_Form1Resolve (0x521EA0)");
}

// originals 0x521200 / 0x5218C0 / 0x521F80 (0x11F bytes each): CellPickup.
extern "C" unsigned char __cdecl PartyAction9_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }
extern "C" unsigned char __cdecl PartyAction10_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }
extern "C" unsigned char __cdecl PartyAction11_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }

// originals 0x5213A0 / 0x521AD0 (0x166 bytes each): Kind30Begin.
extern "C" void __cdecl PartyAction10_Form0Begin(void) { Kind30Begin("PartyAction10_Form0Begin (0x5213A0)"); }
extern "C" void __cdecl PartyAction11_Form0Begin(void) { Kind30Begin("PartyAction11_Form0Begin (0x521AD0)"); }

// original 0x521A20 (0x2A bytes), entry 1 after 0x5252B0 in seven tables:
// Field_State +0x137 = 0 when effect object +0xB's in-use byte is 0; then a
// tail jmp to Sprite_ScriptTick. The index is read unchecked: 0x5252B0 stores
// Effect_FindFree's answer there, 0xFF when no record is free, and the
// original then reads 0x7E9160 (Gfx_PacketPools); ours reads it in place too
// (docs/rest_1d.md section 5, L1).
extern "C" void __cdecl PartyAction_WaitEffectDone(void) {
    const unsigned index = Sprite_Current[0xB];
    if (At(kEffects + index * kEffectStride)[0] == 0) Field_State[0x137] = 0;
    SH_CALL(Sprite_ScriptTick)();
}

// original 0x521C40 (0x33 bytes), eight tables: +9 at 0 - Field_State +0x137 =
// 0 and Field_ScriptFlags &= 0xEFFF; else +9 one down and Field_LeaderStepTick.
// Then a tail jmp to Sprite_ScriptTick.
extern "C" void __cdecl PartyAction_StepCountdown(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    if (count == 0) {
        Field_State[0x137] = 0;
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFFF);
    } else {
        s[9] = static_cast<unsigned char>(count - 1);
        SH_CALL(Field_LeaderStepTick)();
    }
    SH_CALL(Sprite_ScriptTick)();
}

// original 0x522140 (0xA0 bytes), PartyAction12_Form0State0Steps[0]: an even
// direction turned one eighth back; when PartyAction_BlockedAhead answers 0
// that way, two on, and when it answers 0 there too, back to the first turn.
// Sound_PlayEffect(+0x2C + 0x100), the pose (+8 - 1) / 2 + 0x42, then +0xB =
// 0, +6 = 0, +0xA = 8, +3 = 1.
extern "C" void __cdecl PartyAction12_Form0Begin(void) {
    unsigned char* const s = Sprite_Current;
    if ((s[8] & 1) == 0) {
        s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
        if (SH_CALL(PartyAction_BlockedAhead)() == 0) {
            Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 2) & 7);
            if (SH_CALL(PartyAction_BlockedAhead)() == 0) Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] - 2) & 7);
        }
    }
    SH_CALL(Sound_PlayEffect)(SoundOfForm());
    SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x42));
    Sprite_Current[0xB] = 0;
    Sprite_Current[6] = 0;
    Sprite_Current[0xA] = 8;
    Sprite_Current[3] = 1;
}

// original 0x5221E0 (0x140 bytes), PartyAction12_Form0State0Steps[1]: with +0xA
// not 0, one down; when that reaches 0: Field_EffectAhead's object (the byte
// signed: movsx) gets +8 = the direction and +0xA = 1,
// Sound_PlayEffect(0x10B), +6 = its index and +3 one on - and one on again
// below; with none, the point two steps ahead: the object Sprite_ObjectAt(point,
// 0) finds gets +0x80 bit 0 (signed) and Sound_PlayEffect(0x10B), then the cells
// tried with PartyAction12_CellHit; +3 one on. Sprite_ScriptTickOnce every
// time.
extern "C" void __cdecl PartyAction12_Form0Resolve(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[0xA];
    if (count != 0) {
        s[0xA] = static_cast<unsigned char>(count - 1);
        if (Sprite_Current[0xA] == 0) {
            const unsigned char e = SH_CALL(Field_EffectAhead)();
            if (e != 0xFF) {
                unsigned char* const r = EffectChecked(static_cast<unsigned>(static_cast<int>(static_cast<signed char>(e))),
                                                       "PartyAction12_Form0Resolve (0x5221E0)");
                r[8] = Sprite_Current[8];
                r[0xA] = 1;
                SH_CALL(Sound_PlayEffect)(0x10B);
                Sprite_Current[6] = e;
                ++Sprite_Current[3];
            } else {
                U x, z;
                TwoAhead(x, z);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObject(found, true, "PartyAction12_Form0Resolve (0x5221E0)");
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                TryCells(SH_CALL(PartyAction12_CellHit), x, z);
            }
            ++Sprite_Current[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x522320 (0x188 bytes): AreaMap_ByteAt(x, z) -
//   0xF0, 0xF1, 0xF4 - Effect_SpawnAtCellHigh(0, x, z); Rand & 7 above 5: (4,
//          x, z) too; Sound_PlayEffect(0x10B); 1.
//   0xF6, 0xF7 - Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B); then
//          by Rand & 0xF: below 7 - (3, x, z), item 0x29's name (Item_NamePtr(0,
//          0x29), 16 bytes) into Text_Records, Inventory_Add(0, 0x29, 1): taken,
//          Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); +0xB = 2. Above 0xB - (2, x, z),
//          Sprite_FlashClut(0), Char_LoseHp(1, Field_State +0x89 read after
//          it), Msg_OpenSystem(0xD9), +0xB = 1. 7..0xB - nothing more. Each:
//          Field_Request = 2; 1.
//   anything else - 0.
// The callers push a fourth dword to Effect_SpawnAtCellHigh (the cell's byte
// over the z argument's slot, or that slot), which it never reads.
extern "C" unsigned char __cdecl PartyAction12_CellHit(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if ((static_cast<unsigned>(SH_CALL(Rand)()) & 7) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
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
        for (unsigned k = 0; k < 16; k += 4) move_script::SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Sprite_Current[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        const unsigned member = Field_State[0x89];
        SH_CALL(Char_LoseHp)(1, member);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sprite_Current[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

// original 0x5224D0 (0x81 bytes), PartyAction12_Form0State1Steps[1] (and
// 0x65FBF8's): Sprite_ScriptTickOnce answering not 0 - Sprite_EnsureAnimation(+8)
// and +3 = 2. Else with +0xA not 0, one down; reaching 0: effect object +0xB
// gets +8 = the direction; +7 = 1 when it is 0; the object (+0xB read again)
// gets +0xA = +7; Sound_PlayEffect(0x10B). +0xB is read unchecked; ours aborts
// at 20 and above (section 5, L2).
extern "C" void __cdecl PartyAction12_Form0EffectSet(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
        SH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
        Sprite_Current[3] = 2;
        return;
    }
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[0xA];
    if (count == 0) return;
    s[0xA] = static_cast<unsigned char>(count - 1);
    unsigned char* const c = Sprite_Current;
    if (c[0xA] != 0) return;
    const char* const who = "PartyAction12_Form0EffectSet (0x5224D0)";
    const unsigned char direction = c[8];
    EffectChecked(c[0xB], who)[8] = direction;
    if (c[7] == 0) c[7] = 1;
    const unsigned char* const t = Sprite_Current;
    const unsigned char seven = t[7];
    EffectChecked(t[0xB], who)[0xA] = seven;
    SH_CALL(Sound_PlayEffect)(0x10B);
}

void Rest1D_Inject() {
    if (bof3::WantsShadow("rest_1d")) rest_1d::SelfTest();
    BOF3_INJECT(PartyAction9_Form1State1);
    BOF3_INJECT(PartyAction9_Form1Start);
    BOF3_INJECT(PartyFormAction9_Form2);
    BOF3_INJECT(PartyFormAction_TurnToSide);
    BOF3_INJECT(PartyAction9_Form2);
    BOF3_INJECT(PartyAction9_Form2Begin);
    BOF3_INJECT(PartyAction9_Form2Resolve);
    BOF3_INJECT(PartyAction9_CellPickup);
    BOF3_INJECT(PartyFormAction9_ByForm);
    BOF3_INJECT(PartyAction9_ByForm);
    BOF3_INJECT(PartyFormAction10_Form0);
    BOF3_INJECT(PartyAction10_Form0);
    BOF3_INJECT(PartyAction10_Form0Begin);
    BOF3_INJECT(PartyFormAction10_Form1);
    BOF3_INJECT(PartyAction10_Form1);
    BOF3_INJECT(PartyAction10_Form1Begin);
    BOF3_INJECT(PartyAction10_Form1Resolve);
    BOF3_INJECT(PartyAction10_CellPickup);
    BOF3_INJECT(PartyFormAction10_Form2);
    BOF3_INJECT(PartyAction10_Form2);
    BOF3_INJECT(PartyAction_WaitEffectDone);
    BOF3_INJECT(PartyFormAction10_ByForm);
    BOF3_INJECT(PartyAction10_ByForm);
    BOF3_INJECT(PartyFormAction11_Form0);
    BOF3_INJECT(PartyAction11_Form0);
    BOF3_INJECT(PartyAction11_Form0Begin);
    BOF3_INJECT(PartyAction_StepCountdown);
    BOF3_INJECT(PartyFormAction11_Form1);
    BOF3_INJECT(PartyAction11_Form1);
    BOF3_INJECT(PartyAction11_Form1Begin);
    BOF3_INJECT(PartyAction11_Form1Resolve);
    BOF3_INJECT(PartyAction11_CellPickup);
    BOF3_INJECT(PartyFormAction11_ByForm);
    BOF3_INJECT(PartyAction11_ByForm);
    BOF3_INJECT(PartyFormAction12_Form0);
    BOF3_INJECT(PartyAction12_Form0);
    BOF3_INJECT(PartyAction12_Form0State0);
    BOF3_INJECT(PartyAction12_Form0Begin);
    BOF3_INJECT(PartyAction12_Form0Resolve);
    BOF3_INJECT(PartyAction12_CellHit);
    BOF3_INJECT(PartyAction12_Form0State1);
    BOF3_INJECT(PartyAction12_Form0EffectSet);
    BOF3_INJECT(PartyFormAction12_Form1);
    BOF3_INJECT(PartyFormAction12_ByForm);
    BOF3_INJECT(PartyAction12_ByForm);
    BOF3_INJECT(PartyFormAction13_Form0);
}
