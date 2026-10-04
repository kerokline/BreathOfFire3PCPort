// The party sets' field actions of sets 6, 7, 8 and part of 9 - round
// fourteen, wave one, group R1C: the 51 functions 0x51F210..0x520E08 of the cut
// (analysis/round14_cut.tsv; 50 rows and 0x51FA30, code in the band no list
// had). Each read with capstone to its last instruction (docs/rest_1c.md
// section 1). The band repeats a handful of shapes per party set, so many are
// one code with different constants or callees, compared instruction for
// instruction (the doc's section 2):
//
//   the dispatchers (28)        a jmp through a .data table by u16 +0x2C, +2 or +3
//   PartyAction6_Form0Begin     0x51F210 - PartyAction5_Form0Begin's code (field_hidden.cpp)
//   ..._Form0Resolve (3)        0x51F3D0 0x51FF20 0x5205E0 - PartyAction5_Form0Resolve's,
//                               each calling its own set's cell pickup
//   ..._CellPickup (3)          0x51F4B0 (Field_CellPickup's code), 0x520000 0x5206C0
//                               (the zenny's tenfold a twentyfold)
//   ..._Form2Begin (2)          0x51FD40 0x520400 - Form0Begin with the steep test
//                               on the ground's rise, not the slope
//   ..._Form0Begin (3)          0x51FAF0 0x5201A0 0x5208D0 - the jump at a kind-0x30 object
//   ..._Form2Begin / Form1Begin 0x51F670 0x520AA0 - the turn from a blocked way
//   ..._Form2Resolve / ...      0x51F700 0x520B30 - each calling its own cell hit
//   ..._CellHit (2)             0x51F880 0x520C80
//   shared                      0x51F850 0x51FA30 0x51FC80 0x520350 0x520840
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. Where the original would write
// through an index past its table (an effect object's or a sprite object's),
// or jump through a dispatch table to what is not code, ours aborts with a
// message (the round-nine rule); no ordinary caller hands such an index
// (section 5 of the doc). The one unchecked read ordinary play reaches
// (PartyAction_WaitEffect's effect index) is reproduced in place.
#include "game/rest_1c.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using Handler = void (__cdecl*)();
using CellFn = unsigned char (__cdecl*)(unsigned, unsigned);

constexpr U kTextLo = 0x401000, kTextHi = 0x5C3000;   // .text: what a dispatch table may hold
constexpr U kSteps = 0x6697B0;                         // Field_DirectionSteps: 8 rows of two longs
constexpr U kStep3 = 0x6697C8, kStep5 = 0x6697D8;      // rows 3 and 5, read by address
constexpr U kSloped = 0x903850;                        // DamageScratch's first byte: AreaMap_Slope's "sloped" flag
constexpr U kObjectFlag = 0x7DEF00;                    // Sprite_Objects + 0x80
constexpr U kExtraFlag = 0x802080;                     // Sprite_ObjectsExtra + 0x80
constexpr U kObjectStride = 0xA4;
constexpr U kScriptFlagsHigh = 0x9039A3;               // Field_ScriptFlags' high byte
constexpr U kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;

// A dispatch table's entry, read in place with the index unchecked as the
// original reads it; where the word is not code (an index past the table and
// past the runs of handlers after it) ours aborts, where the original jumps
// there. While the fuzz runs the table holds the harness's recorders.
Handler Entry(U table, unsigned index, const char* who) {
    const U at = table + 4u * index;
    const U entry = static_cast<U>(Long(At(at)));
    if (!scenario_harness::g_active && (entry < kTextLo || entry >= kTextHi))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    (unsigned)entry, (unsigned)at);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}
void ByForm(U table, const char* who) { Entry(table, Word(Sprite_Current + 0x2C), who)(); }
void ByState(U table, const char* who) { Entry(table, Sprite_Current[2], who)(); }
void ByStep(U table, const char* who) { Entry(table, Sprite_Current[3], who)(); }

// Field_DirectionSteps' row for a direction, read in place with the direction
// unmasked (`shl eax, 3` on the zero-extended byte), as the originals read it.
U StepX(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8))); }
U StepZ(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8 + 4))); }

// The animation offset of a direction as the originals compute it: (d - 1) / 2
// as a signed division (cdq, sub, sar), so direction 0 gives 0; plus a base, as
// a byte (`add al`). For any direction byte the upper bytes are 0.
unsigned char Turned(unsigned char d, unsigned base) {
    return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2 + static_cast<int>(base));
}

// A direction byte turned and masked: (d + n) & 7, as `add` / `sub` and `and 7`.
void Turn(int n) { Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + n) & 7); }

unsigned char Sloped() { return At(kSloped)[0]; }

// Bit 0 of +0x80 of the object Sprite_ObjectAt found: Sprite_Objects 0..0x1D,
// Sprite_ObjectsExtra 0x1E..0x21 (Sprite_ObjectAt answers those or 0xFF). The
// originals index unchecked (0x51F700 / Form0Resolve the byte signed, 0x51FAF0
// unsigned); ours aborts on any other byte.
void MarkObject(unsigned char found, const char* who) {
    if (found > 0x21)
        bof3::Fatal("%s: Sprite_ObjectAt answered 0x%02X, past the 34 objects; the original writes bit 0 of +0x80 there unchecked",
                    who, found);
    if (found < 0x1E) At(kObjectFlag + found * kObjectStride)[0] |= 1;
    else At(kExtraFlag + (found - 0x1E) * kObjectStride)[0] |= 1;
}

// An Effect_Objects record by an index the original writes through unchecked;
// ours aborts past the 20 (Field_EffectAhead and PartyAction_Kind30Ahead, the
// only sources, answer 0..19 or 0xFF, which the callers test first).
unsigned char* EffectToWrite(int index, const char* who) {
    if (index < 0 || index >= static_cast<int>(kEffectCount))
        bof3::Fatal("%s: effect object index %d is past Effect_Objects' 20 records; the original writes there unchecked", who,
                    index);
    return Effect_Objects + index * kEffectStride;
}

// The side probe the Form0Begin shape makes inline in directions 3 and 5 (the
// rows read by address, the direction pushed as an immediate): the point one
// step that way from Sprite_Current (re-read); the slope there steep (the
// sloped flag set, its low word above 0x40 signed) and the ground there above
// the sprite's height word (both s16): +0x2B = 0. PartyAction_SideProbes'
// probe, and PartyAction5_Form0Begin's ProbeSide.
void ProbeSide(U row, unsigned d) {
    const unsigned char* s = Sprite_Current;
    const U x = static_cast<U>(Long(s + 0x34)) + static_cast<U>(Long(At(row)));
    const U z = static_cast<U>(Long(s + 0x38)) + static_cast<U>(Long(At(row + 4)));
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (Sloped() == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    if (static_cast<short>(Word(s + 0x3E)) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}

// The first turn of the Begin shapes: an even direction +8 is turned one
// eighth back; when `ahead` finds nothing (al 0) two on, and when it finds
// nothing there either, back to the first. An odd direction is kept.
void FirstTurn(unsigned char (__cdecl* ahead)(void)) {
    if ((Sprite_Current[8] & 1) != 0) return;
    Turn(-1);
    if (ahead() != 0) return;
    Turn(2);
    if (ahead() != 0) return;
    Turn(-2);
}
unsigned char __cdecl TargetAhead() { return SH_CALL(PartyAction_TargetAhead)(); }
unsigned char __cdecl BlockedAhead() { return SH_CALL(PartyAction_BlockedAhead)(); }

// The not-steep half of both Begin shapes: +0x2B = 1, the side probes in
// directions 3 and 5, Sound_PlayEffect(u16 +0x2C + 0x100),
// Sprite_EnsureAnimation(the turn + 0x42), +0xA = 5.
void BeginLevel() {
    Sprite_Current[0x2B] = 1;
    ProbeSide(kStep3, 3);
    ProbeSide(kStep5, 5);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sprite_Current + 0x2C) + 0x100));
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x42));
    Sprite_Current[0xA] = 5;
}
// The steep half: Sprite_EnsureAnimation(the turn + 0x46), +2 one on.
void BeginSteep() {
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x46));
    ++Sprite_Current[2];
}
// Both shapes' end: +0xB = 0, +2 one on (so a steep slope moves +2 by two).
void BeginEnd() {
    Sprite_Current[0xB] = 0;
    ++Sprite_Current[2];
}

// PartyAction5_Form0Begin's code (0x51F210 is a copy of 0x51E930 instruction
// for instruction): the first turn by PartyAction_TargetAhead, then the slope
// one step ahead in +8 (MapView_SlopeAt with the direction byte) decides.
void Form0Begin() {
    FirstTurn(&TargetAhead);
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (Sloped() != 0 && static_cast<short>(slope) > 0x40) BeginSteep();
    else BeginLevel();
    BeginEnd();
}

// 0x51FD40 / 0x520400: the same, but steep is the ground one step ahead
// (MapView_GroundAt, its low word) above the height word +0x3E by more than
// 0x40 (s16) with the sloped flag set; MapView_SlopeAt is called after the
// ground, with the direction re-read, for its flag alone (its answer is
// dropped). Sprite_Current is re-read after the ground call.
void Form2Begin() {
    FirstTurn(&TargetAhead);
    const unsigned char* s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(s + 0x3E)));
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), s[8]);
    if (Sloped() != 0 && rise > 0x40) BeginSteep();
    else BeginLevel();
    BeginEnd();
}

// PartyAction5_Form0Resolve's code (0x51F3D0, 0x51FF20, 0x5205E0 differ from
// 0x51EAF0 only in the pickup they call): +0xA counted down; at 0 the point two
// steps ahead; the object there marked; the pickup on the point's cell, then
// on the cell one on in x (x with a fraction), then in z (z with one), each
// only while none found; +2 one on. Sprite_ScriptTickOnce every time. The
// originals pass the cells as dwords over their own uninitialised stack; each
// pickup reads 16 bits of them (docs/field_hidden.md section 3).
void Form0Resolve(CellFn pickup, const char* who) {
    --Sprite_Current[0xA];
    if (Sprite_Current[0xA] == 0) {
        const unsigned char* const s = Sprite_Current;
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
        ++Sprite_Current[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// Field_CellPickup's code (0x51F4B0 is a copy of 0x51EBD0; 0x520000 and
// 0x5206C0 differ only in the tenfold, `mov cl, 0x14`): what lies in the map
// cell (x, z) - 0xF2 with an effect object free: Effect_SpawnAtCell(0, x, z),
// Rand & 0xF of 13, 14, 15 finds zenny 2 (5 on 15), `times` that when
// Field_InputFlags has bit 1 or 2 and a second Rand & 3 is 0 (a byte product):
// Field_GiveZenny, Effect_SpawnAtCell(1, x, z); +0xB = 1. 0xF8:
// Effect_SpawnAtCell(0, x, z), item 0x56's name into Text_Records, Inventory_Add
// (0, 0x56, 1): Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
// Msg_OpenSystem(3); Field_Request 2, +0xB = 1. Both clear the cell and answer
// 1; anything else answers 0.
unsigned char CellPickup(unsigned x, unsigned z, unsigned times) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned char>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * times);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sprite_Current[0xB] = 1;
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
        Sprite_Current[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x51F670 / 0x520AA0: the first turn by PartyAction_BlockedAhead (turned back
// while the way is blocked), PartyAction_SideProbes, Sprite_EnsureAnimation(
// the turn + 0x42), +0xB = 0, +0xA = 0xB, +3 = 1.
void BlockedBegin() {
    FirstTurn(&BlockedAhead);
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x42));
    Sprite_Current[0xB] = 0;
    Sprite_Current[0xA] = 0xB;
    Sprite_Current[3] = 1;
}

// 0x51F700 / 0x520B30 (they differ only in the cell hit they call): +0xA
// counted down when not 0; at 0: Sound_PlayEffect(u16 +0x2C + 0x100); an
// effect object of kind 0x17 ahead (Field_EffectAhead) gets Sound_PlayEffect(
// 0x10B), its +8 = the direction, its +0xA = 1, the sprite's +6 = its index
// and +3 one on; with none, the point two steps ahead: the object there marked
// (with Sound_PlayEffect(0x10B)), then the cell hit on the point's cell and
// the cells one on in x / z across a fraction, as Form0Resolve's pickup. Then
// +3 one on (so an effect object found moves it by two). Sprite_ScriptTickOnce
// every time.
void BlockedResolve(CellFn hit, const char* who) {
    unsigned char* const s = Sprite_Current;
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        if (Sprite_Current[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sprite_Current + 0x2C) + 0x100));
            const unsigned char k = SH_CALL(Field_EffectAhead)();
            if (k != 0xFF) {
                SH_CALL(Sound_PlayEffect)(0x10B);
                unsigned char* const c = Sprite_Current;
                unsigned char* const e = EffectToWrite(static_cast<signed char>(k), who);
                e[8] = c[8];
                e[0xA] = 1;
                c[6] = k;
                ++Sprite_Current[3];
            } else {
                const unsigned char* const c = Sprite_Current;
                const unsigned d = c[8];
                const U x = static_cast<U>(Long(c + 0x34)) + 2u * StepX(d);
                const U z = static_cast<U>(Long(c + 0x38)) + 2u * StepZ(d);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObject(found, who);
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                const unsigned cx = x >> 16, cz = z >> 16;
                if (hit(cx, cz) == 0) {
                    bool done = false;
                    if ((x & 0xFFFF) != 0) done = hit(cx + 1, cz) != 0;
                    if (!done && (z & 0xFFFF) != 0) hit(cx, cz + 1);
                }
            }
            ++Sprite_Current[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51F880 / 0x520C80 (identical but for their address): what lies in the map
// cell (x, z) - 0xF0, 0xF1, 0xF4: Effect_SpawnAtCellHigh(0, x, z), a second
// (state 4) when Rand & 7 is above 5, Sound_PlayEffect(0x10B); 0xF6, 0xF7:
// Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B), then by Rand & 0xF
// - below 7: Effect_SpawnAtCellHigh(3, x, z), item 0x29's name into
// Text_Records, Inventory_Add(0, 0x29, 1): Sound_PlayEffect(0x106) and
// Msg_OpenSystem(2), else Msg_OpenSystem(3); +0xB = 2 - 7..0xB: nothing more -
// above 0xB: Effect_SpawnAtCellHigh(2, x, z), Sprite_FlashClut(0),
// Char_LoseHp(1, Field_State +0x89), Msg_OpenSystem(0xD9), +0xB = 1; Field_Request
// 2 in all three. Each answers 1; anything else 0. The original stores the cell
// byte over its own z argument's low byte and pushes that dword as a fourth
// argument Effect_SpawnAtCellHigh does not read; no caller reads it again.
unsigned char CellHit(unsigned x, unsigned z) {
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
        Sprite_Current[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Field_State[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sprite_Current[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

// 0x51FAF0 / 0x5201A0 / 0x5208D0 (identical but for their address): the
// kind-0x30 effect object lined up ahead (PartyAction_Kind30Ahead). With one:
// when a member stands on it or beyond it (PartyAction_MemberOnEffect, then
// PartyAction_MemberBeyondEffect, its index as given), Field_State +0x137 = 0
// and no more; else its +0xB = 1, Sound_PlayEffect(u16 +0x2C + 0x100),
// Sprite_EnsureAnimation(+8 + 8, a byte), Field_State +0x128 = 2,
// Field_ScriptFlags |= 0x1000, Field_JumpStart, +9 one down,
// Field_LeaderStepTick, Field_State +0x137 = 1, +2 one on. With none: unless
// bit 0 of Field_State +0x138 is set, the object two steps ahead
// (Sprite_ObjectAt, margin 1) is marked; Field_State +0x137 = 0. Field_State is
// re-read after every call.
void Kind30Begin(const char* who) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Field_State[0x137] = 0;
            return;
        }
        const unsigned char* const s = Sprite_Current;
        EffectToWrite(k, who)[0xB] = 1;
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(s + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 8));
        Field_State[0x128] = 2;
        At(kScriptFlagsHigh)[0] |= 0x10;
        SH_CALL(Field_JumpStart)();
        --Sprite_Current[9];
        SH_CALL(Field_LeaderStepTick)();
        Field_State[0x137] = 1;
        ++Sprite_Current[2];
        return;
    }
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
    if ((Field_State[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 1);
        if (found != 0xFF) MarkObject(found, who);
    }
    Field_State[0x137] = 0;
}

}  // namespace

#define R1C_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// The dispatchers: Field_FormActions[6..8] and Field_ActionBySet[6..8] by the
// form, the u16 +0x2C; a form's state table by +2; a state's step table by +3.
// Each a jmp through its .data table, the index unchecked (docs section 3).
// ===========================================================================

// original 0x51FA70: Field_FormActions[6] - PartyFormAction6_Forms 0x65FC88 by u16 +0x2C.
R1C_EXPORT void __cdecl PartyFormAction6_ByForm(void) { ByForm(0x65FC88, "PartyFormAction6_ByForm (0x51FA70)"); }
// original 0x51FA90: Field_ActionBySet[6] - PartyAction6_Forms 0x65FC94 by u16 +0x2C.
R1C_EXPORT void __cdecl PartyAction6_ByForm(void) { ByForm(0x65FC94, "PartyAction6_ByForm (0x51FA90)"); }
// original 0x520120: Field_FormActions[7] - PartyFormAction7_Forms 0x65FCD8.
R1C_EXPORT void __cdecl PartyFormAction7_ByForm(void) { ByForm(0x65FCD8, "PartyFormAction7_ByForm (0x520120)"); }
// original 0x520140: Field_ActionBySet[7] - PartyAction7_Forms 0x65FCE4.
R1C_EXPORT void __cdecl PartyAction7_ByForm(void) { ByForm(0x65FCE4, "PartyAction7_ByForm (0x520140)"); }
// original 0x5207E0: Field_FormActions[8] - PartyFormAction8_Forms 0x65FD34.
R1C_EXPORT void __cdecl PartyFormAction8_ByForm(void) { ByForm(0x65FD34, "PartyFormAction8_ByForm (0x5207E0)"); }
// original 0x520800: Field_ActionBySet[8] - PartyAction8_Forms 0x65FD40.
R1C_EXPORT void __cdecl PartyAction8_ByForm(void) { ByForm(0x65FD40, "PartyAction8_ByForm (0x520800)"); }

// original 0x51F5D0: set 6's form action, form 1 - PartyFormAction6_Form1States 0x65FC3C by +2.
R1C_EXPORT void __cdecl PartyFormAction6_Form1(void) { ByState(0x65FC3C, "PartyFormAction6_Form1 (0x51F5D0)"); }
// original 0x51F5F0: set 6's action, form 1 - PartyAction6_Form1States 0x65FC48 by +2.
R1C_EXPORT void __cdecl PartyAction6_Form1(void) { ByState(0x65FC48, "PartyAction6_Form1 (0x51F5F0)"); }
// original 0x51F610: set 6's form action, form 2 - PartyFormAction6_Form2States 0x65FC54 by +2.
R1C_EXPORT void __cdecl PartyFormAction6_Form2(void) { ByState(0x65FC54, "PartyFormAction6_Form2 (0x51F610)"); }
// original 0x51F630: set 6's action, form 2 - PartyAction6_Form2States 0x65FC60 by +2.
R1C_EXPORT void __cdecl PartyAction6_Form2(void) { ByState(0x65FC60, "PartyAction6_Form2 (0x51F630)"); }
// original 0x51F650: its state 0 - PartyAction6_Form2State0Steps 0x65FC68 by +3.
R1C_EXPORT void __cdecl PartyAction6_Form2State0(void) { ByStep(0x65FC68, "PartyAction6_Form2State0 (0x51F650)"); }
// original 0x51FA10: its state 1 - PartyAction6_Form2State1Steps 0x65FC7C by +3.
R1C_EXPORT void __cdecl PartyAction6_Form2State1(void) { ByStep(0x65FC7C, "PartyAction6_Form2State1 (0x51FA10)"); }
// original 0x51FAB0: set 7's form action, form 0 - 0x65FCA0 by +2.
R1C_EXPORT void __cdecl PartyFormAction7_Form0(void) { ByState(0x65FCA0, "PartyFormAction7_Form0 (0x51FAB0)"); }
// original 0x51FAD0: set 7's action, form 0 - 0x65FCAC by +2.
R1C_EXPORT void __cdecl PartyAction7_Form0(void) { ByState(0x65FCAC, "PartyAction7_Form0 (0x51FAD0)"); }
// original 0x51FC60: set 7's form action, form 1 - 0x65FCB4 by +2.
R1C_EXPORT void __cdecl PartyFormAction7_Form1(void) { ByState(0x65FCB4, "PartyFormAction7_Form1 (0x51FC60)"); }
// original 0x51FD00: set 7's form action, form 2 - 0x65FCC0 by +2.
R1C_EXPORT void __cdecl PartyFormAction7_Form2(void) { ByState(0x65FCC0, "PartyFormAction7_Form2 (0x51FD00)"); }
// original 0x51FD20: set 7's action, form 2 - 0x65FCCC by +2.
R1C_EXPORT void __cdecl PartyAction7_Form2(void) { ByState(0x65FCCC, "PartyAction7_Form2 (0x51FD20)"); }
// original 0x520160: set 8's form action, form 0 - 0x65FCF0 by +2.
R1C_EXPORT void __cdecl PartyFormAction8_Form0(void) { ByState(0x65FCF0, "PartyFormAction8_Form0 (0x520160)"); }
// original 0x520180: set 8's action, form 0 - 0x65FCFC by +2.
R1C_EXPORT void __cdecl PartyAction8_Form0(void) { ByState(0x65FCFC, "PartyAction8_Form0 (0x520180)"); }
// original 0x520310: set 8's form action, form 1 - 0x65FD04 by +2.
R1C_EXPORT void __cdecl PartyFormAction8_Form1(void) { ByState(0x65FD04, "PartyFormAction8_Form1 (0x520310)"); }
// original 0x520330: set 8's action, form 1 - 0x65FD10 by +2.
R1C_EXPORT void __cdecl PartyAction8_Form1(void) { ByState(0x65FD10, "PartyAction8_Form1 (0x520330)"); }
// original 0x5203C0: set 8's form action, form 2 - 0x65FD1C by +2.
R1C_EXPORT void __cdecl PartyFormAction8_Form2(void) { ByState(0x65FD1C, "PartyFormAction8_Form2 (0x5203C0)"); }
// original 0x5203E0: set 8's action, form 2 - 0x65FD28 by +2.
R1C_EXPORT void __cdecl PartyAction8_Form2(void) { ByState(0x65FD28, "PartyAction8_Form2 (0x5203E0)"); }
// original 0x520820: set 9's form action, form 0 (R1D's 0x521320 by +0x2C) - 0x65FD4C by +2.
R1C_EXPORT void __cdecl PartyFormAction9_Form0(void) { ByState(0x65FD4C, "PartyFormAction9_Form0 (0x520820)"); }
// original 0x5208B0: set 9's action, form 0 (R1D's 0x521340) - 0x65FD58 by +2.
R1C_EXPORT void __cdecl PartyAction9_Form0(void) { ByState(0x65FD58, "PartyAction9_Form0 (0x5208B0)"); }
// original 0x520A40: set 9's form action, form 1 - 0x65FD60 by +2.
R1C_EXPORT void __cdecl PartyFormAction9_Form1(void) { ByState(0x65FD60, "PartyFormAction9_Form1 (0x520A40)"); }
// original 0x520A60: set 9's action, form 1 - 0x65FD6C by +2.
R1C_EXPORT void __cdecl PartyAction9_Form1(void) { ByState(0x65FD6C, "PartyAction9_Form1 (0x520A60)"); }
// original 0x520A80: its state 0 - PartyAction9_Form1State0Steps 0x65FD74 by +3.
R1C_EXPORT void __cdecl PartyAction9_Form1State0(void) { ByStep(0x65FD74, "PartyAction9_Form1State0 (0x520A80)"); }

// ===========================================================================
// The state handlers
// ===========================================================================

// original 0x51F210: set 6's action, form 0, state 0 (PartyAction5_Form0Begin's
// code): the first turn by PartyAction_TargetAhead; the slope one step ahead
// steep - Sprite_EnsureAnimation(the turn + 0x46), +2 one on - or not: +0x2B =
// 1, the side probes 3 and 5, Sound_PlayEffect(u16 +0x2C + 0x100),
// Sprite_EnsureAnimation(the turn + 0x42), +0xA = 5. Then +0xB = 0, +2 one on.
R1C_EXPORT void __cdecl PartyAction6_Form0Begin(void) { Form0Begin(); }
// original 0x51FD40: set 7's action, form 2, state 0 - the steep test on the
// ground's rise one step ahead (Form2Begin above).
R1C_EXPORT void __cdecl PartyAction7_Form2Begin(void) { Form2Begin(); }
// original 0x520400: set 8's action, form 2, state 0 - 0x51FD40's code.
R1C_EXPORT void __cdecl PartyAction8_Form2Begin(void) { Form2Begin(); }

// original 0x51F4B0: set 6's cell pickup (Field_CellPickup's code; the zenny's
// tenfold), called by PartyAction6_Form0Resolve.
R1C_EXPORT unsigned char __cdecl PartyAction6_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z, 10); }
// original 0x520000: set 7's - the zenny's twentyfold.
R1C_EXPORT unsigned char __cdecl PartyAction7_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z, 20); }
// original 0x5206C0: set 8's - the zenny's twentyfold.
R1C_EXPORT unsigned char __cdecl PartyAction8_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z, 20); }

// original 0x51F3D0: set 6's action, form 0, state 1 (PartyAction5_Form0Resolve's
// code) with set 6's pickup.
R1C_EXPORT void __cdecl PartyAction6_Form0Resolve(void) {
    Form0Resolve(SH_CALL(PartyAction6_CellPickup), "PartyAction6_Form0Resolve (0x51F3D0)");
}
// original 0x51FF20: set 7's action, form 2, state 1, with set 7's pickup.
R1C_EXPORT void __cdecl PartyAction7_Form2Resolve(void) {
    Form0Resolve(SH_CALL(PartyAction7_CellPickup), "PartyAction7_Form2Resolve (0x51FF20)");
}
// original 0x5205E0: set 8's action, form 2, state 1, with set 8's pickup.
R1C_EXPORT void __cdecl PartyAction8_Form2Resolve(void) {
    Form0Resolve(SH_CALL(PartyAction8_CellPickup), "PartyAction8_Form2Resolve (0x5205E0)");
}

// original 0x51F670: set 6's action, form 2, state 0, step 0 - BlockedBegin above.
R1C_EXPORT void __cdecl PartyAction6_Form2Begin(void) { BlockedBegin(); }
// original 0x520AA0: set 9's action, form 1, state 0, step 0 - 0x51F670's code.
R1C_EXPORT void __cdecl PartyAction9_Form1Begin(void) { BlockedBegin(); }

// original 0x51F880: set 6's cell hit (CellHit above), called by
// PartyAction6_Form2Resolve.
R1C_EXPORT unsigned char __cdecl PartyAction6_CellHit(unsigned x, unsigned z) { return CellHit(x, z); }
// original 0x520C80: set 9's - 0x51F880's code.
R1C_EXPORT unsigned char __cdecl PartyAction9_CellHit(unsigned x, unsigned z) { return CellHit(x, z); }

// original 0x51F700: set 6's action, form 2, state 0, step 1, with set 6's cell hit.
R1C_EXPORT void __cdecl PartyAction6_Form2Resolve(void) {
    BlockedResolve(SH_CALL(PartyAction6_CellHit), "PartyAction6_Form2Resolve (0x51F700)");
}
// original 0x520B30: set 9's action, form 1, state 0, step 1, with set 9's cell hit.
R1C_EXPORT void __cdecl PartyAction9_Form1Resolve(void) {
    BlockedResolve(SH_CALL(PartyAction9_CellHit), "PartyAction9_Form1Resolve (0x520B30)");
}

// original 0x51FAF0: set 7's action, form 0, state 0 - Kind30Begin above.
R1C_EXPORT void __cdecl PartyAction7_Form0Begin(void) { Kind30Begin("PartyAction7_Form0Begin (0x51FAF0)"); }
// original 0x5201A0: set 8's - 0x51FAF0's code.
R1C_EXPORT void __cdecl PartyAction8_Form0Begin(void) { Kind30Begin("PartyAction8_Form0Begin (0x5201A0)"); }
// original 0x5208D0: set 9's - 0x51FAF0's code.
R1C_EXPORT void __cdecl PartyAction9_Form0Begin(void) { Kind30Begin("PartyAction9_Form0Begin (0x5208D0)"); }

// original 0x51FA30 (code in no list): set 6's action, form 2, state 1, step 0 -
// PartyAction_SideProbes, Sprite_EnsureAnimation(the turn + 0x42), +0xA = 0xB,
// +3 = 1.
R1C_EXPORT void __cdecl PartyAction6_Form2Probe(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x42));
    Sprite_Current[0xA] = 0xB;
    Sprite_Current[3] = 1;
}

// original 0x51F850 (nine .data cells, the step tables of sets 2, 5, 6, 9 and
// more): Sprite_ScriptTickOnce; when it answers non-zero,
// Sprite_SetAnimation(+8) and +3 one on.
R1C_EXPORT void __cdecl PartyAction_TickThenFace(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
    ++Sprite_Current[3];
}

// original 0x520840 (34 .data cells: state 0 of most form actions): the side
// the direction +8 is nearer, 3 or 5 (|d - 3| against |d - 5| as ints; a tie
// is 3): +0xB = Sprite_TurnSense(that side), +3 = it; then +9 = 2, +2 one on.
R1C_EXPORT void __cdecl PartyAction_TurnToSide(void) {
    const int d = Sprite_Current[8];
    const int to3 = d - 3 < 0 ? 3 - d : d - 3;
    const int to5 = d - 5 < 0 ? 5 - d : d - 5;
    const unsigned char side = to3 > to5 ? 5 : 3;
    const unsigned char sense = SH_CALL(Sprite_TurnSense)(side);
    Sprite_Current[0xB] = sense;
    Sprite_Current[3] = side;
    Sprite_Current[9] = 2;
    ++Sprite_Current[2];
}

// original 0x51FC80 (31 .data cells: state 1 of most form actions): at the side
// (+8 equal to +3): Sprite_EnsureAnimation(0x40) at 3, (0x41) at 5, then +2 one
// on; else +9 one down, and at 0: +9 = 2, +8 = (+8 + the sense +0xB) & 7,
// Sprite_EnsureAnimation(+8).
R1C_EXPORT void __cdecl PartyAction_TurnStep(void) {
    unsigned char* s = Sprite_Current;
    const unsigned char side = s[3];
    if (s[8] == side) {
        if (side == 3 || side == 5) {
            SH_CALL(Sprite_EnsureAnimation)(side == 3 ? 0x40 : 0x41);
            s = Sprite_Current;
        }
        ++s[2];
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (Sprite_Current[9] != 0) return;
    Sprite_Current[9] = 2;
    Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] + Sprite_Current[0xB]);
    Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] & 7);
    SH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
}

// original 0x520350 (seven .data cells: state 2 of form 1 in sets 1, 2, 5 and
// 8 and more): with +7 set, Sprite_ScriptTickOnce and, when it answers
// non-zero, Sprite_EnsureAnimation(+8), +7 = 0; else Sprite_ScriptTickOnce and,
// when it answers non-zero and effect object +0xB is no longer in use (+0 0),
// +0x2B = 0 and Field_State +0x137 = 0.
//
// As the original has it: the effect index +0xB is read unchecked - the state
// before (R1B's 0x51DE20, and its likes) stores Effect_FindFree's answer there,
// 0xFF when no record is free, and moves on all the same; the byte 0xFF * 0x80
// past Effect_Objects is then read (docs section 5). Reproduced in place: the
// read does not fault and ordinary play can reach it.
R1C_EXPORT void __cdecl PartyAction_WaitEffect(void) {
    if (Sprite_Current[7] != 0) {
        if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
        SH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
        Sprite_Current[7] = 0;
        return;
    }
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    unsigned char* const s = Sprite_Current;
    if (At(bof3::addr::Effect_Objects + static_cast<U>(s[0xB]) * kEffectStride)[0] != 0) return;
    s[0x2B] = 0;
    Field_State[0x137] = 0;
}

void Rest1C_Inject() {
    if (bof3::WantsShadow("rest_1c")) rest_1c::SelfTest();
    BOF3_INJECT(PartyAction6_Form0Begin);
    BOF3_INJECT(PartyAction6_Form0Resolve);
    BOF3_INJECT(PartyAction6_CellPickup);
    BOF3_INJECT(PartyFormAction6_Form1);
    BOF3_INJECT(PartyAction6_Form1);
    BOF3_INJECT(PartyFormAction6_Form2);
    BOF3_INJECT(PartyAction6_Form2);
    BOF3_INJECT(PartyAction6_Form2State0);
    BOF3_INJECT(PartyAction6_Form2Begin);
    BOF3_INJECT(PartyAction6_Form2Resolve);
    BOF3_INJECT(PartyAction_TickThenFace);
    BOF3_INJECT(PartyAction6_CellHit);
    BOF3_INJECT(PartyAction6_Form2State1);
    BOF3_INJECT(PartyAction6_Form2Probe);
    BOF3_INJECT(PartyFormAction6_ByForm);
    BOF3_INJECT(PartyAction6_ByForm);
    BOF3_INJECT(PartyFormAction7_Form0);
    BOF3_INJECT(PartyAction7_Form0);
    BOF3_INJECT(PartyAction7_Form0Begin);
    BOF3_INJECT(PartyFormAction7_Form1);
    BOF3_INJECT(PartyAction_TurnStep);
    BOF3_INJECT(PartyFormAction7_Form2);
    BOF3_INJECT(PartyAction7_Form2);
    BOF3_INJECT(PartyAction7_Form2Begin);
    BOF3_INJECT(PartyAction7_Form2Resolve);
    BOF3_INJECT(PartyAction7_CellPickup);
    BOF3_INJECT(PartyFormAction7_ByForm);
    BOF3_INJECT(PartyAction7_ByForm);
    BOF3_INJECT(PartyFormAction8_Form0);
    BOF3_INJECT(PartyAction8_Form0);
    BOF3_INJECT(PartyAction8_Form0Begin);
    BOF3_INJECT(PartyFormAction8_Form1);
    BOF3_INJECT(PartyAction8_Form1);
    BOF3_INJECT(PartyAction_WaitEffect);
    BOF3_INJECT(PartyFormAction8_Form2);
    BOF3_INJECT(PartyAction8_Form2);
    BOF3_INJECT(PartyAction8_Form2Begin);
    BOF3_INJECT(PartyAction8_Form2Resolve);
    BOF3_INJECT(PartyAction8_CellPickup);
    BOF3_INJECT(PartyFormAction8_ByForm);
    BOF3_INJECT(PartyAction8_ByForm);
    BOF3_INJECT(PartyFormAction9_Form0);
    BOF3_INJECT(PartyAction_TurnToSide);
    BOF3_INJECT(PartyAction9_Form0);
    BOF3_INJECT(PartyAction9_Form0Begin);
    BOF3_INJECT(PartyFormAction9_Form1);
    BOF3_INJECT(PartyAction9_Form1);
    BOF3_INJECT(PartyAction9_Form1State0);
    BOF3_INJECT(PartyAction9_Form1Begin);
    BOF3_INJECT(PartyAction9_Form1Resolve);
    BOF3_INJECT(PartyAction9_CellHit);
}
