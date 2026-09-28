// World 1's areas 38..41: the PSX's BIN/WORLD01/AREA038..041.EMI compiled into
// the exe at 0x4053B0..0x406650 (Area_Descriptors entries 38..41). Round ten,
// group AR1A: the band's 48 functions, none ours before, each read to its last
// instruction with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w1a.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The two
// dispatchers through an area's .data state table abort past the table where
// the original would jump through whatever the next dwords hold (the owner's
// rule for an unchecked index, round9 doc section 6; no route reaches it).
// Area40_TileLit's record walk runs on for ever on a step of 0 or one that
// overshoots, as the original does (docs/area_w1a.md section 6). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies.
#include "game/area_w1a.h"

#include "game/draw_pool.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1a::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
// Field_ActiveMember's word +0x8A less 2 (area 39's handlers).
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }
// Field_State's word +0x12E less 2 (area 41's states).
void LeaderStep() { AddWord(Field_State + 0x12E, -2); }

void SetByte(U x, U z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// `fld dword; fstp dword`: a copy, except that a signalling NaN comes out
// quiet (area 40's corners; d3d_draw.cpp's X87Copy).
void X87Copy(unsigned char* to, const unsigned char* from) {
    float r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(*reinterpret_cast<const float*>(from)) : "st");
    __builtin_memcpy(to, &r, 4);
}

// Area 38's two effects (0x405410, 0x4054D0): with the party-list byte 0,
// Sprite_Current is made party record `record` (and left so), its +0xB the
// free effect slot; the effect record +0 = 1, +5 = 6, +6 = 3, +0xC = 0, +0x10
// the signed byte Area38_EffectBytes[the list byte, read again], +0x2E / +0x30
// the record's words. The slot is re-read from the record for every store.
void SpawnForMember(U list, U record) {
    if (B(list) != 0) return;
    Sprite_Current = Mem(record);
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return;
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 6;
    EffectAt(cur[0xB])[6] = 3;
    SetLong(EffectAt(cur[0xB]) + 0xC, 0);
    const auto step = static_cast<signed char>(B(at::kArea38EffectBytes + B(list)));
    SetLong(EffectAt(cur[0xB]) + 0x10, step);
    SetWord(EffectAt(cur[0xB]) + 0x2E, Word(cur + 0x2E));
    SetWord(EffectAt(cur[0xB]) + 0x30, Word(cur + 0x30));
}

// Area 40's four lever choices (0x405810..0x4058D0): message 0xFFFF; answer
// 0: lever flag `flag` of the row 0x903FB0, the gate redrawn, sound 0x203.
void Lever(unsigned flag) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(Flags_Set)(Mem(at::kLeverRow), flag);
    AH_CALL(Area40_SetGate)();
    AH_CALL(Sound_PlayEffect)(0x203);
}

// The tail's way out (area 40's choice 5 and tail 0x10): Field_ScriptFlags
// &= 0xEFF9, the tail kind and state 0.
void TailOff() {
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFF9);
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}

// The 28 gate cells, x 0x41..0x44 by z 3..9 (Area40_SetGate, Area40_Init).
void Gate(unsigned value) {
    for (unsigned n = 0; n < 7; ++n) {
        const U z = n + 3;
        SetByte(0x41, z, value);
        SetByte(0x42, z, value);
        SetByte(0x43, z, value);
        SetByte(0x44, z, value);
    }
}

bool InGrid(U x, U z) {
    return static_cast<std::uint16_t>(x - at::kGridX) < at::kGridSide && static_cast<std::uint16_t>(z - at::kGridZ) < at::kGridSide;
}

// Area 41's tint pair (0x406560 up, 0x4064C0 down): +0x34 moved by +0xC, the
// running script's tint record (MoveScript_TintRecords by Field_State +0x149)
// bytes +2..+4 moved by `d`, the index re-read for each.
void TintBy(unsigned char d) {
    unsigned char* const cur = Sprite_Current;
    AddLong(cur + 0x34, static_cast<U>(Long(cur + 0xC)));
    unsigned char* const leader = Field_State;
    for (unsigned off = 2; off <= 4; ++off) MoveScript_TintRecords[leader[0x149] * 12u + off] += d;
}

// Area 41's countdown on +0xA (states 1 and 2): at 0, +0xA = 8 and state
// `next`; Field_State's word +0x12E less 2 either way.
void CountDown(unsigned char next) {
    unsigned char* cur = Sprite_Current;
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[0xA] = 8;
        Sprite_Current[4] = next;
    }
    LeaderStep();
}

}  // namespace

// ===========================================================================
// Area 38 (descriptor 0x5F2208): four handlers.
// ===========================================================================

// original 0x4053B0 (Area38_Handlers[0]; PSX 0x801F2C04): three cells of
// column 0xC zeroed, z 0x1E, 0x1C, 0x1D.
extern "C" void __cdecl Area38_ClearCells(void) {
    SetByte(0xC, 0x1E, 0);
    SetByte(0xC, 0x1C, 0);
    SetByte(0xC, 0x1D, 0);
}

// original 0x4053E0 (Area38_Handlers[1]; PSX 0x801F2C4C): the same, 0xC0.
extern "C" void __cdecl Area38_SetCellsC0(void) {
    SetByte(0xC, 0x1E, 0xC0);
    SetByte(0xC, 0x1C, 0xC0);
    SetByte(0xC, 0x1D, 0xC0);
}

// original 0x405410 (Area38_Handlers[2]; PSX 0x801F2C94): party-list byte
// 0x904063, party record 1.
extern "C" void __cdecl Area38_SpawnEffectMember1(void) { SpawnForMember(at::kPartyList1, at::kPartyRecord1); }

// original 0x4054D0 (Area38_Handlers[3]; PSX 0x801F2E60): 0x904064, record 2.
extern "C" void __cdecl Area38_SpawnEffectMember2(void) { SpawnForMember(at::kPartyList2, at::kPartyRecord2); }

// ===========================================================================
// Area 39 (descriptor 0x5F3E60): eleven handlers of its own and a two-state
// machine.
// ===========================================================================

// original 0x405590 (Area39_Handlers[0]; PSX 0x801F2C04): Area39_States by
// Sprite_Current[4].
extern "C" void __cdecl Area39_Run(void) {
    StateEntry("Area39_Run", at::kArea39States, at::kArea39StateCount, Sprite_Current[4])();
}

// original 0x4055B0 (Area39_States[0]): +0xA = 0x40, state 1; the member's
// word +0x8A less 2.
extern "C" void __cdecl Area39_DriftStart(void) {
    Sprite_Current[0xA] = 0x40;
    Sprite_Current[4] = 1;
    MemberStep();
}

// original 0x4055E0 (Area39_States[1]): while +0xA: +0x34 by the signed step
// Area39_DriftSteps[+0xA & 0xF] << 11, +0x38 by its negation << 11, +0xA less
// 1, the member's word +0x8A less 2; at 0, state 0.
extern "C" void __cdecl Area39_DriftStep(void) {
    unsigned char* cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    AddLong(cur + 0x34, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kArea39DriftSteps + (cur[0xA] & 0xF))))) << 11);
    cur = Sprite_Current;
    const auto step = static_cast<std::int32_t>(static_cast<signed char>(B(at::kArea39DriftSteps + (cur[0xA] & 0xF))));
    AddLong(cur + 0x38, (0u - static_cast<U>(step)) << 11);
    cur = Sprite_Current;
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    MemberStep();
}

// original 0x405650 (Area39_Handlers[3]; PSX 0x801F2D54): a free effect slot
// gets +0 = 1, the kind +5 = 0x36, +9 = 0; none: the member's word +0x8A less 2.
extern "C" void __cdecl Area39_SpawnEffect36(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) {
        MemberStep();
        return;
    }
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x36;
    e[9] = 0;
}

// original 0x4056A0 (Area39_Handlers[4]; PSX 0x801F2DE4).
extern "C" void __cdecl Area39_ClearCells(void) {
    SetByte(0x43, 0x36, 0);
    SetByte(0x44, 0x36, 0);
}

// original 0x4056C0 (Area39_Handlers[5]; PSX 0x801F2E1C).
extern "C" void __cdecl Area39_SetCells51(void) {
    SetByte(0x58, 0xD, 0x51);
    SetByte(0x59, 0xD, 0x51);
}

// original 0x4056E0 (Area39_Handlers[6]; PSX 0x801F2E54).
extern "C" void __cdecl Area39_SetCells50(void) {
    SetByte(0x58, 0xD, 0x50);
    SetByte(0x59, 0xD, 0x50);
}

// original 0x405700 (Area39_Handlers[10]; PSX 0x801F2EC4).
extern "C" void __cdecl Area39_PlayMusicA3(void) { AH_CALL(Music_Play)(0xA3, 8); }

// original 0x405710 (Area39_Handlers[11]; PSX 0x801F2EEC): Field_ScriptFlags'
// low byte |= 8.
extern "C" void __cdecl Area39_SetScriptFlag8(void) { B(0x9039A2) = static_cast<unsigned char>(B(0x9039A2) | 8); }

// original 0x405720 (Area39_Handlers[12]; PSX 0x801F2F0C): the first key-item
// byte of 0xE becomes 4; none: 0x591900(4), the key item 4 added.
extern "C" void __cdecl Area39_SwapKeyItemE(void) {
    for (unsigned i = 0; i < at::kKeyItemCount; ++i)
        if (B(at::kKeyItems + i) == 0xE) {
            B(at::kKeyItems + i) = 4;
            return;
        }
    AH_AT(void (__cdecl*)(unsigned), at::kKeyItemAdd)(4);
}

// original 0x405750 (Area39_Handlers[13], also area 130's +0x3C[4]; PSX
// 0x801F2F68).
extern "C" void __cdecl Area39_FadeOutMusic(void) { AH_CALL(Music_FadeOutStop)(0x1E); }

// original 0x405760 (Area39_Handlers[14]; PSX 0x801F2F94): counter 1 = the
// first party-list byte is not 4.
extern "C" void __cdecl Area39_Counter1Not4(void) { B(at::kCounter1) = B(at::kPartyList0) != 4 ? 1 : 0; }

// ===========================================================================
// Area 40 (descriptor 0x5F4BD8): ten choices (two of them the handlers), an
// init, and a 4 x 4 puzzle - a mode tail, an arrive hook and five helpers.
// ===========================================================================

// original 0x405770 (Area40_Choices[8] = Area40_Handlers[0]; PSX 0x801F2C04):
// a free field object's index (0x57CD90) to the scratch word; none (0xFF):
// the running script's position back 2 (the op again next frame); else event
// op Area40_PlaceOp places it.
extern "C" void __cdecl Area40_PlaceObject(void) {
    const unsigned char free = AH_AT(unsigned char (__cdecl*)(), at::kFreeObject)();
    SetWord(Mem(at::kScratchWord), free);
    if (free == 0xFF) {
        AddWord(MoveScript_Object + 0xA, -2);
        return;
    }
    AH_CALL(EventOp_0x)(Mem(at::kArea40PlaceOp));
}

// original 0x4057A0 (Area40_Choices[9] = Area40_Handlers[1]; PSX
// 0x801F2C74): the leader's +0x89 of 2 moves the running object half a cell
// in x.
extern "C" void __cdecl Area40_NudgeObject(void) {
    if (Field_State[0x89] == 2) AddLong(Sprite_Current + 0x34, 0x8000);
}

// original 0x4057C0 (Area40_Choices[0]): message 0xFFFF, the byte after
// MoveScript_Var7 0x1C (answer 0) or 0x1E; flag 5 of the chapter's row; then
// Field_ScriptFlags bits 3 and 7 and Field_StatusBits bit 6 cleared.
extern "C" void __cdecl Area40_ChoiceFlag5(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    unsigned char* const row = Ptr(at::kFlagRow);
    SetMessage(0xFFFF);
    B(at::kVar7Step) = answer != 0 ? 0x1E : 0x1C;
    AH_CALL(Flags_Set)(row, 5);
    const unsigned char status = Field_StatusBits;
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFF77);
    Field_StatusBits = static_cast<unsigned char>(status & 0xBF);
}

// originals 0x405810, 0x405850, 0x405890, 0x4058D0 (Area40_Choices[1..4]):
// the levers, flags 0xB, 0xA, 9, 8 of the row 0x903FB0.
extern "C" void __cdecl Area40_ChoiceLeverB(void) { Lever(0xB); }
extern "C" void __cdecl Area40_ChoiceLeverA(void) { Lever(0xA); }
extern "C" void __cdecl Area40_ChoiceLever9(void) { Lever(9); }
extern "C" void __cdecl Area40_ChoiceLever8(void) { Lever(8); }

// original 0x405910 (called by the lever choices): the four lever flags (the
// low nibble of 0x903FB1) at 5 open the gate (its 28 cells 0, al 1); any
// other closes it (0xA4, al 0).
extern "C" unsigned char __cdecl Area40_SetGate(void) {
    if ((B(at::kLeverNibble) & 0xF) == 5) {
        Gate(0);
        return 1;
    }
    Gate(0xA4);
    return 0;
}

// original 0x4059B0 (Area40_Choices[5]): message 0xFFFF; answer not 0: the
// tail off. Answer 0: the leader's +0x89 of 2 opens message 0x3C; else, no
// tail armed, ScriptFlags_Set40 and the tail kind 0x10 (Area40_TailPuzzle) at
// state 0.
extern "C" void __cdecl Area40_ChoiceTail16(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) {
        TailOff();
        return;
    }
    if (B(at::kLeaderByte89) == 2) {
        SetMessage(0x3C);
        return;
    }
    if (B(at::kTailKind) != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x10;
    B(at::kTailState) = 0;
}

// original 0x405A10 (Field_ModeTailKinds[0x10]): the puzzle's frame, by the
// s8 state. 0: ScriptFlags_Clear40; story flag 0x27 clear: Party_DropIn(4),
// Field_ScriptFlags |= 0x1000, state 1; set: state 0xA. 1: counter 0 at 0x63:
// Field_ScriptFlags |= 6, counter 0 = 0, state 2. 2: with Field_Request 0 the
// held buttons to the scratch word and their top nibble remapped through
// Area40_InputMap; the grid drawn. Then, every state: unless the area is 0x28,
// the state not 0xA and Cond_ByteFD 6, the tail off.
extern "C" void __cdecl Area40_TailPuzzle(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        AH_CALL(ScriptFlags_Clear40)();
        if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0x27) != 0) {
            B(at::kTailState) = 0xA;
            break;
        }
        AH_CALL(Party_DropIn)(4);
        B(at::kScriptFlagsHigh) = static_cast<unsigned char>(B(at::kScriptFlagsHigh) | 0x10);
        B(at::kTailState) = 1;
        break;
    case 1:
        if (B(at::kCounter0) != 0x63) break;
        B(0x9039A2) = static_cast<unsigned char>(B(0x9039A2) | 6);
        B(at::kCounter0) = 0;
        B(at::kTailState) = 2;
        break;
    case 2:
        if (Field_Request == 0) {
            SetWord(Mem(at::kScratchWord), Word(Mem(at::kInputHeld)));
            const U held = static_cast<U>(Long(Mem(at::kInputHeld)));
            const U top = B(at::kArea40InputMap + ((held & 0xFFFF) >> 12));
            SetWord(Mem(at::kInputHeld), top << 12 | (held & 0xFFF));
        }
        AH_CALL(Area40_DrawGrid)();
        break;
    default: break;
    }
    if (Game_AreaNumber == 0x28 && B(at::kTailState) != 0xA && Cond_ByteFD == 6) return;
    TailOff();
}

// original 0x405B00 (Area_ArriveHook's case for area 40): with Cond_ByteFD 6,
// the leader's +0x2B 0, story flag 0x27 clear and the puzzle's tail armed:
// the grid's map bytes cleared; a cell (the high words) inside the grid is
// toggled - lit (Area40_TileLit) switched off by MoveCmd_TestFC, else on by
// MoveCmd_TestFB - the cell behind marked and sound 0x205; outside it, unless
// the pattern is complete, every grid cell switched off. al 0 always. Every
// cell word goes out with its high half stale in the original (the callees
// read 16 bits).
extern "C" unsigned char __cdecl Area40_ArriveHook(unsigned long x, unsigned long z) {
    if (Cond_ByteFD != 6) return 0;
    if (Field_State[0x2B] != 0) return 0;
    if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0x27) != 0) return 0;
    if (B(at::kTailKind) != 0x10) return 0;
    AH_CALL(Area40_ClearGrid)();
    const auto cx = static_cast<std::uint16_t>(x >> 16), cz = static_cast<std::uint16_t>(z >> 16);
    if (InGrid(cx, cz)) {
        if (AH_CALL(Area40_TileLit)(cx, cz) == 0) AH_CALL(MoveCmd_TestFB)(static_cast<short>(cx), static_cast<short>(cz));
        else AH_CALL(MoveCmd_TestFC)(static_cast<short>(cx), static_cast<short>(cz));
        AH_CALL(Area40_MarkBehind)(x, z);
        AH_CALL(Sound_PlayEffect)(0x205);
        return 0;
    }
    if (AH_CALL(Area40_PuzzleSolved)() != 0) return 0;
    for (unsigned c = 0; c < at::kGridSide; ++c)
        for (unsigned r = 0; r < at::kGridSide; ++r)
            AH_CALL(MoveCmd_TestFC)(static_cast<short>(at::kGridX + c), static_cast<short>(at::kGridZ + r));
    return 0;
}

// original 0x405BF0 (called by the arrive hook and Area40_PuzzleSolved): is
// the map cell (x, z) (words) lit? The view-ring cell as MoveCmd_TestFB finds
// it (the words less MapView_Origin in 16 bits; outside the 56-cell ring, 0);
// its run of area-block records, each stepping by its byte +2 dwords until the
// step lands on the run's last dword; the first record of kind 0x23..0x25
// whose patch entry (its low word + AreaMap_PatchBase) is of kind 0x8000
// under 0xF000 answers that entry's bit 0. Else 0. Reads only. A step of 0 or
// one past the end walks on for ever, as the original (section 6).
extern "C" unsigned char __cdecl Area40_TileLit(unsigned long x, unsigned long z) {
    const std::int32_t dx = static_cast<std::int16_t>(static_cast<std::uint16_t>(x - static_cast<std::uint16_t>(MapView_Origin[0])));
    const std::int32_t dz = static_cast<std::int16_t>(static_cast<std::uint16_t>(z - static_cast<std::uint16_t>(MapView_Origin[1])));
    std::int32_t row = dz + dx;
    if (row >= 0x38 || row < 0) return 0;
    row += MapView_Row + 1;
    if (row >= 0x38) row -= 0x38;
    const std::int32_t diagonal = dx - dz;
    if (diagonal >= 0x38 || diagonal < 0) return 0;
    std::int32_t column = diagonal / 2 + MapView_Column + 1;
    if (column >= 0x1C) column -= 0x1C;
    const U cell = Word(Mem(at::kViewCells + static_cast<U>(column + row * 28) * 2u));
    if (cell == 0) return 0;
    const U index = cell + (static_cast<U>(Long(Mem(at::kCellBase))) & 0xFFFF);
    U walk = at::kMapHeader + index * 4u;
    const U count = static_cast<U>(Long(Mem(walk - 4))) >> 16;
    const U end = walk + count * 4u - 4u;
    if (walk == end) return 0;
    const U patch_base = static_cast<U>(Long(Mem(at::kPatchBase))) & 0xFFFF;
    do {
        const U record = static_cast<U>(Long(Mem(walk)));
        const U kind = record & 0xFF000000u;
        if (kind == 0x23000000u || kind == 0x24000000u || kind == 0x25000000u) {
            const U entry = static_cast<U>(Long(Mem(at::kMapHeader + ((record & 0xFFFF) + patch_base) * 4u)));
            if ((entry & 0xF000) == 0x8000) return static_cast<unsigned char>(entry & 1);
        }
        walk += ((record >> 16) & 0xFF) * 4u;
    } while (walk != end);
    return 0;
}

// original 0x405D30 (called by the arrive hook): each grid cell's
// Area40_TileLit against Area40_Pattern (row by z); any differing: 0. All
// equal: story flag 0x27, the four cells beside the grid opened (0xA1 row
// 0x1C, 0xC0 row 0x1D), the tail state 0xA, MoveCmd_TestFB(0x92, 0x1D), sound
// 0x103, al 1.
extern "C" unsigned char __cdecl Area40_PuzzleSolved(void) {
    for (unsigned r = 0; r < at::kGridSide; ++r)
        for (unsigned c = 0; c < at::kGridSide; ++c)
            if (AH_CALL(Area40_TileLit)(at::kGridX + c, at::kGridZ + r) != B(at::kArea40Pattern + r * 4 + c)) return 0;
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x27);
    SetByte(0x92, 0x1C, 0xA1);
    SetByte(0x93, 0x1C, 0xA1);
    SetByte(0x92, 0x1D, 0xC0);
    SetByte(0x93, 0x1D, 0xC0);
    B(at::kTailState) = 0xA;
    AH_CALL(MoveCmd_TestFB)(0x92, 0x1D);
    AH_CALL(Sound_PlayEffect)(0x103);
    return 1;
}

// original 0x405E30 (called by the arrive hook): the grid's 16 map bytes 0,
// row by row.
extern "C" void __cdecl Area40_ClearGrid(void) {
    for (unsigned r = 0; r < at::kGridSide; ++r)
        for (unsigned c = 0; c < at::kGridSide; ++c) SetByte(at::kGridX + c, at::kGridZ + r, 0);
}

// original 0x405E80 (called by the arrive hook with the arrival's x, z): the
// cell one step behind the leader (its direction, byte +8 & 7, through the
// cell-step pairs 0x66971C) - the high words less the step in 16 bits,
// written back over the arguments - inside the grid gets map byte 0x50. The
// original hands AreaMap_SetByte its argument dwords as they then are: x's
// new high word with z's low word above it, z's new high word with two bytes
// of the caller's frame above it (read in 16 bits).
extern "C" void __cdecl Area40_MarkBehind(unsigned long x, unsigned long z) {
    const unsigned d = B(at::kLeaderByte8) & 7;
    const auto sx = static_cast<std::int16_t>(static_cast<signed char>(B(at::kCellDelta + d * 2)));
    const auto sz = static_cast<std::int16_t>(static_cast<signed char>(B(at::kCellDelta + d * 2 + 1)));
    const auto cx = static_cast<std::uint16_t>((x >> 16) - static_cast<std::uint16_t>(sx));
    const auto cz = static_cast<std::uint16_t>((z >> 16) - static_cast<std::uint16_t>(sz));
    if (!InGrid(cx, cz)) return;
    SetByte(cx | static_cast<U>(z) << 16, cz, 0x50);
}

// original 0x405ED0 (called by Area40_TailPuzzle in state 2): each grid cell
// whose map byte is 0x50 and that has a map item gets a semi-transparent flat
// quad at Gfx_PacketNext: its four corners the item's (DrawItems, the half
// Gfx_BufferIndex 0 picks) x and y floats, its colour grey, pulsing with the
// frame (|((Frame_Counter >> 1) & 0xF) - 8| * 0x1F), linked at the cell with
// size 0x38.
extern "C" void __cdecl Area40_DrawGrid(void) {
    for (unsigned r = 0; r < at::kGridSide; ++r)
        for (unsigned c = 0; c < at::kGridSide; ++c) {
            const U x = at::kGridX + c, z = at::kGridZ + r;
            if (AH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)) != 0x50) continue;
            const U item = static_cast<U>(AH_CALL(MapView_ItemAt)(static_cast<long>(x), static_cast<long>(z)));
            if (item == 0) continue;
            unsigned char* const p = Gfx_PacketNext;
            AH_CALL(Gpu_SetPolyF4)(p);
            AH_CALL(Gpu_SetSemiTrans)(p, 1);
            static constexpr unsigned kFrom[8] = {0x8, 0xC, 0x18, 0x1C, 0x28, 0x2C, 0x38, 0x3C};
            static constexpr unsigned kTo[8] = {0x8, 0xC, 0x14, 0x18, 0x20, 0x24, 0x2C, 0x30};
            for (unsigned i = 0; i < 8; ++i) {
                const U half = (Gfx_BufferIndex == 0 ? 1u : 0u) + item * 2u;
                X87Copy(p + kTo[i], Mem(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(draw_pool::Items())) +
                                        half * at::kDrawItemStride + kFrom[i]));   // DIV-0062: the pool's array
            }
            const std::int32_t pulse = static_cast<std::int32_t>((Frame_Counter >> 1) & 0xF) - 8;
            const auto grey = static_cast<unsigned char>((pulse < 0 ? -pulse : pulse) * 0x1F);
            p[6] = grey;
            p[5] = grey;
            p[4] = grey;
            AH_CALL(MapView_LinkPrimAt)(x << 16, z << 16, 0, 0x38);
        }
}

// original 0x4060C0 (area 40 +0x40; PSX 0x801F3A00): on entry. Cond_ByteFD not
// 1: the lever flags cleared and the gate closed. Cond_ByteFD 6 with story
// flag 0x27 clear: the cells beside the grid closed (row 0x1D 0, row 0x1C
// 0x50). Entered from zone 6: only with Cond_ByteFD 2, Field_ScriptFlags bit
// 12, flag 0x27 and counter 0 cleared. From zone 4: the pattern's cells lit.
extern "C" void __cdecl Area40_Init(void) {
    if (Cond_ByteFD != 1) {
        B(at::kLeverNibble) = static_cast<unsigned char>(B(at::kLeverNibble) & 0xF0);
        Gate(0xA4);
    }
    if (Cond_ByteFD == 6 && AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0x27) == 0) {
        SetByte(0x92, 0x1D, 0);
        SetByte(0x93, 0x1D, 0);
        SetByte(0x92, 0x1C, 0x50);
        SetByte(0x93, 0x1C, 0x50);
    }
    if (B(at::kEntryZone) == 6) {
        if (Cond_ByteFD != 2) return;
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFFF);
        AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 0x27);
        B(at::kCounter0) = 0;
    }
    if (B(at::kEntryZone) != 4) return;
    for (unsigned r = 0; r < at::kGridSide; ++r)
        for (unsigned c = 0; c < at::kGridSide; ++c)
            if (B(at::kArea40Pattern + r * 4 + c) != 0)
                AH_CALL(MoveCmd_TestFB)(static_cast<short>(at::kGridX + c), static_cast<short>(at::kGridZ + r));
}

// ===========================================================================
// Area 41 (descriptor 0x5F5F10): fifteen choices (the last five the
// handlers), a four-state machine, two object triggers.
// ===========================================================================

// original 0x406200 (Area41_Choices[0]): message 0xFFFF; answer 0:
// Party_DropIn(1), counter 1 = 0xA; else Party_DropIn(2), 0x14.
extern "C" void __cdecl Area41_ChoiceDropIn12(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 0) {
        AH_CALL(Party_DropIn)(1);
        B(at::kCounter1) = 0xA;
        return;
    }
    AH_CALL(Party_DropIn)(2);
    B(at::kCounter1) = 0x14;
}

// original 0x406240 (Area41_Choices[1]): answer 0: Party_DropIn(3), counter 1
// = 0x1E; else Party_DropIn(4), the byte 0x903804 points at (read after the
// call) 0, counter 1 = 0x28.
extern "C" void __cdecl Area41_ChoiceDropIn34(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 0) {
        AH_CALL(Party_DropIn)(3);
        B(at::kCounter1) = 0x1E;
        return;
    }
    AH_CALL(Party_DropIn)(4);
    Ptr(at::kFocusObject)[0] = 0;
    B(at::kCounter1) = 0x28;
}

// original 0x406280 (Area41_Choices[3]): the message Area41_GiveMessages[the
// signed answer] (unchecked: any answer but 0 and 1 reads the words either
// side); answer 0 with flag 6 of the chapter's row clear: sound 0x106, item
// 0x28 of category 0 given, the flag set (the row pointer read again).
extern "C" void __cdecl Area41_ChoiceGiveItem(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    const auto index = static_cast<std::int32_t>(static_cast<signed char>(answer));
    SetMessage(Word(Mem(at::kArea41GiveMessages + static_cast<U>(index) * 2u)));
    if (answer != 0) return;
    if (AH_CALL(Flags_Test)(Ptr(at::kFlagRow), 6) != 0) return;
    AH_CALL(Sound_PlayEffect)(0x106);
    AH_CALL(Inventory_Add)(0, 0x28, 1);   // the original pushes a fourth word, 0, never read
    AH_CALL(Flags_Set)(Ptr(at::kFlagRow), 6);
}

// original 0x4062E0 (Area41_Choices[4]): answer 0 with items 0x49, 0x3F, 0x27
// and 0x1D of category 0 all held: each taken (0x591B60), message 0x62; any
// missing: message 0x63 and the mark. Answer not 0: 0x64 and the mark.
extern "C" void __cdecl Area41_ChoiceTrade(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x64);
        B(at::kAnswerMark) = 6;
        return;
    }
    static constexpr unsigned kItems[4] = {0x49, 0x3F, 0x27, 0x1D};
    for (unsigned item : kItems)
        if (AH_CALL(Inventory_Count)(0, item, 0) == 0) {
            SetMessage(0x63);
            B(at::kAnswerMark) = 6;
            return;
        }
    for (unsigned item : kItems) AH_AT(void (__cdecl*)(unsigned, unsigned, unsigned, unsigned), at::kInventoryTake)(0, item, 1, 0);
    SetMessage(0x62);
}

// original 0x4063A0 (Area41_Choices[7] and [8]): answer not 0: message 0x64
// and the mark; else 0xFFFF.
extern "C" void __cdecl Area41_ChoiceConfirm64(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x64);
        B(at::kAnswerMark) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x4063D0 (Area41_Choices[12] = Area41_Handlers[2]; PSX
// 0x801F3E3C): Area41_States by Sprite_Current[4].
extern "C" void __cdecl Area41_Run(void) {
    StateEntry("Area41_Run", at::kArea41States, at::kArea41StateCount, Sprite_Current[4])();
}

// original 0x4063F0 (Area41_States[0]): the step +0xC = 0x4000, a tint step
// up (Area41_TintUp), then +0xA = 7 and state 1 (Sprite_Current read again
// for each); the leader's word +0x12E less 2.
extern "C" void __cdecl Area41_TintStart(void) {
    SetLong(Sprite_Current + 0xC, 0x4000);
    AH_CALL(Area41_TintUp)();
    Sprite_Current[0xA] = 7;
    Sprite_Current[4] = 1;
    LeaderStep();
}

// original 0x406430 (Area41_States[1]): a tint step up; +0xA down, at 0 the
// slide (state 2) for 8.
extern "C" void __cdecl Area41_TintRise(void) {
    AH_CALL(Area41_TintUp)();
    CountDown(2);
}

// original 0x406470 (Area41_States[2]): +0x34 by +0xC; +0xA down, at 0 the
// fall (state 3) for 8.
extern "C" void __cdecl Area41_Slide(void) {
    unsigned char* const cur = Sprite_Current;
    AddLong(cur + 0x34, static_cast<U>(Long(cur + 0xC)));
    CountDown(3);
}

// original 0x4064C0 (Area41_States[3]): a tint step down; +0xA down, at 0
// state 0 (and the leader's word left); else the leader's word +0x12E less 2.
extern "C" void __cdecl Area41_TintFall(void) {
    TintBy(0xFE);
    unsigned char* cur = Sprite_Current;
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    LeaderStep();
}

// original 0x406560 (called by TintStart and TintRise): +0x34 by +0xC, the
// tint record's three bytes up 2.
extern "C" void __cdecl Area41_TintUp(void) { TintBy(2); }

// original 0x4065D0 (Area41_Choices[13] = Area41_Handlers[3]; PSX 0x801F41F8).
extern "C" void __cdecl Area41_ClearCells(void) {
    SetByte(0x40, 0x28, 0);
    SetByte(0x40, 0x27, 0);
}

// original 0x4065F0 (Area41_Choices[14] = Area41_Handlers[4]; PSX 0x801F4230).
extern "C" void __cdecl Area41_PlaceKind2(void) { AH_CALL(Kind2_Place)(8); }

// original 0x406600 (Field_ObjectTriggers id 29, (object, flags) ignored):
// ScriptFlags_Set40, the mode tail kind 4 with sub-kind 0xA; al 0.
extern "C" unsigned char __cdecl Area41_Trigger29(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 0xA;
    return 0;
}

// original 0x406620 (Field_ObjectTriggers ids 59 and 60): the story flag the
// byte 0x5F5F2D + the object's +0x86 names (Area41_TriggerFlags for 59, 60;
// the flags argument is not read), sound 0x201; al 0.
extern "C" unsigned char __cdecl Area41_TriggerFlag(unsigned char* object, unsigned char*) {
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), B(at::kArea41TriggerBase + object[0x86]));
    AH_CALL(Sound_PlayEffect)(0x201);
    return 0;
}

void AreaW1a_Inject() {
    if (bof3::WantsShadow("area_w1a")) area_w1a::SelfTest();
    BOF3_INJECT(Area38_ClearCells);
    BOF3_INJECT(Area38_SetCellsC0);
    BOF3_INJECT(Area38_SpawnEffectMember1);
    BOF3_INJECT(Area38_SpawnEffectMember2);
    BOF3_INJECT(Area39_Run);
    BOF3_INJECT(Area39_DriftStart);
    BOF3_INJECT(Area39_DriftStep);
    BOF3_INJECT(Area39_SpawnEffect36);
    BOF3_INJECT(Area39_ClearCells);
    BOF3_INJECT(Area39_SetCells51);
    BOF3_INJECT(Area39_SetCells50);
    BOF3_INJECT(Area39_PlayMusicA3);
    BOF3_INJECT(Area39_SetScriptFlag8);
    BOF3_INJECT(Area39_SwapKeyItemE);
    BOF3_INJECT(Area39_FadeOutMusic);
    BOF3_INJECT(Area39_Counter1Not4);
    BOF3_INJECT(Area40_PlaceObject);
    BOF3_INJECT(Area40_NudgeObject);
    BOF3_INJECT(Area40_ChoiceFlag5);
    BOF3_INJECT(Area40_ChoiceLeverB);
    BOF3_INJECT(Area40_ChoiceLeverA);
    BOF3_INJECT(Area40_ChoiceLever9);
    BOF3_INJECT(Area40_ChoiceLever8);
    BOF3_INJECT(Area40_SetGate);
    BOF3_INJECT(Area40_ChoiceTail16);
    BOF3_INJECT(Area40_TailPuzzle);
    BOF3_INJECT(Area40_ArriveHook);
    BOF3_INJECT(Area40_TileLit);
    BOF3_INJECT(Area40_PuzzleSolved);
    BOF3_INJECT(Area40_ClearGrid);
    BOF3_INJECT(Area40_MarkBehind);
    BOF3_INJECT(Area40_DrawGrid);
    BOF3_INJECT(Area40_Init);
    BOF3_INJECT(Area41_ChoiceDropIn12);
    BOF3_INJECT(Area41_ChoiceDropIn34);
    BOF3_INJECT(Area41_ChoiceGiveItem);
    BOF3_INJECT(Area41_ChoiceTrade);
    BOF3_INJECT(Area41_ChoiceConfirm64);
    BOF3_INJECT(Area41_Run);
    BOF3_INJECT(Area41_TintStart);
    BOF3_INJECT(Area41_TintRise);
    BOF3_INJECT(Area41_Slide);
    BOF3_INJECT(Area41_TintFall);
    BOF3_INJECT(Area41_TintUp);
    BOF3_INJECT(Area41_ClearCells);
    BOF3_INJECT(Area41_PlaceKind2);
    BOF3_INJECT(Area41_Trigger29);
    BOF3_INJECT(Area41_TriggerFlag);
}
