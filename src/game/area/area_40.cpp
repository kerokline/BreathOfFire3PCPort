// Moved here by tools/move_funcs.py from area_w1a.cpp (docs/source-layout-round.md).
#include "game/area/area_40.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1a_callees.h"
#include "game/draw_pool.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

void SetByte(U x, U z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// `fld dword; fstp dword`: a copy, except that a signalling NaN comes out
// quiet (area 40's corners; d3d_draw.cpp's X87Copy).
void X87Copy(unsigned char* to, const unsigned char* from) {
    float r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(*reinterpret_cast<const float*>(from)) : "st");
    __builtin_memcpy(to, &r, 4);
}

// Area 40's four lever choices (0x405810..0x4058D0): message 0xFFFF; answer
// 0: lever flag `flag` of the row 0x903FB0, the gate redrawn, sound 0x203.
void Lever(unsigned flag) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0x20), flag);
    AH_CALL(Area40_SetGate)();
    AH_CALL(Sound_PlayEffect)(0x203);
}

// The tail's way out (area 40's choice 5 and tail 0x10): Field_ScriptFlags
// &= 0xEFF9, the tail kind and state 0.
void TailOff() {
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFF9);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
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

}  // namespace

// original 0x405770 (Area40_Choices[8] = Area40_Handlers[0]; PSX 0x801F2C04):
// a free field object's index (0x57CD90) to the scratch word; none (0xFF):
// the running script's position back 2 (the op again next frame); else event
// op Area40_PlaceOp places it.
extern "C" void __cdecl Area40_PlaceObject(void) {
    const unsigned char free = AH_AT(unsigned char (__cdecl*)(), at::kFreeObject.v)();
    SetWord(Mem(::bof3::Img(at::kScratchWord.v)), free);
    if (free == 0xFF) {
        AddWord(MoveScript_Object + 0xA, -2);
        return;
    }
    AH_CALL(EventOp_0x)(Mem(::bof3::Img(Area40_PlaceOp_addr)));
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
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    unsigned char* const row = Ptr(::bof3::Img(Chapter_FlagRow_addr));
    SetMessage(0xFFFF);
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = answer != 0 ? 0x1E : 0x1C;
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
    if ((B(::bof3::Img(Cond_Flags_addr) + 0x21) & 0xF) == 5) {
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
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) {
        TailOff();
        return;
    }
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) == 2) {
        SetMessage(0x3C);
        return;
    }
    if (B(::bof3::Img(AreaW0a_TailKind_addr)) != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x10;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x405A10 (Field_ModeTailKinds[0x10]): the puzzle's frame, by the
// s8 state. 0: ScriptFlags_Clear40; story flag 0x27 clear: Party_DropIn(4),
// Field_ScriptFlags |= 0x1000, state 1; set: state 0xA. 1: counter 0 at 0x63:
// Field_ScriptFlags |= 6, counter 0 = 0, state 2. 2: with Field_Request 0 the
// held buttons to the scratch word and their top nibble remapped through
// Area40_InputMap; the grid drawn. Then, every state: unless the area is 0x28,
// the state not 0xA and Cond_ByteFD 6, the tail off.
extern "C" void __cdecl Area40_TailPuzzle(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        AH_CALL(ScriptFlags_Clear40)();
        if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x27) != 0) {
            B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
            break;
        }
        AH_CALL(Party_DropIn)(4);
        B(::bof3::Img(at::kScriptFlagsHigh.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kScriptFlagsHigh.v)) | 0x10);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        break;
    case 1:
        if (B(::bof3::Img(at::kCounter0.v)) != 0x63) break;
        B(::bof3::Img(Field_ScriptFlags_addr)) = static_cast<unsigned char>(B(::bof3::Img(Field_ScriptFlags_addr)) | 6);
        B(::bof3::Img(at::kCounter0.v)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        break;
    case 2:
        if (Field_Request == 0) {
            SetWord(Mem(::bof3::Img(at::kScratchWord.v)), Word(Mem(::bof3::Img(Input_Held_addr))));
            const U held = static_cast<U>(Long(Mem(::bof3::Img(Input_Held_addr))));
            const U top = B(::bof3::Img(Area40_InputMap_addr) + ((held & 0xFFFF) >> 12));
            SetWord(Mem(::bof3::Img(Input_Held_addr)), top << 12 | (held & 0xFFF));
        }
        AH_CALL(Area40_DrawGrid)();
        break;
    default: break;
    }
    if (Game_AreaNumber == 0x28 && B(::bof3::Img(AreaW0c_TailState_addr)) != 0xA && Cond_ByteFD == 6) return;
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
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x27) != 0) return 0;
    if (B(::bof3::Img(AreaW0a_TailKind_addr)) != 0x10) return 0;
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
    const U cell = Word(Mem(::bof3::Img(MapView_Cells_addr) + static_cast<U>(column + row * 28) * 2u));
    if (cell == 0) return 0;
    const U index = cell + (static_cast<U>(Long(Mem(::bof3::Img(AreaMap_CellBase_addr)))) & 0xFFFF);
    U walk = ::bof3::Img(AreaMap_Header_addr) + index * 4u;
    const U count = static_cast<U>(Long(Mem(walk - 4))) >> 16;
    const U end = walk + count * 4u - 4u;
    if (walk == end) return 0;
    const U patch_base = static_cast<U>(Long(Mem(::bof3::Img(AreaMap_PatchBase_addr)))) & 0xFFFF;
    do {
        const U record = static_cast<U>(Long(Mem(walk)));
        const U kind = record & 0xFF000000u;
        if (kind == 0x23000000u || kind == 0x24000000u || kind == 0x25000000u) {
            const U entry = static_cast<U>(Long(Mem(::bof3::Img(AreaMap_Header_addr) + ((record & 0xFFFF) + patch_base) * 4u)));
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
            if (AH_CALL(Area40_TileLit)(at::kGridX + c, at::kGridZ + r) != B(::bof3::Img(Area40_Pattern_addr) + r * 4 + c)) return 0;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x27);
    SetByte(0x92, 0x1C, 0xA1);
    SetByte(0x93, 0x1C, 0xA1);
    SetByte(0x92, 0x1D, 0xC0);
    SetByte(0x93, 0x1D, 0xC0);
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
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
    const unsigned d = B(::bof3::Img(ObjTrio_addr) + 0x8) & 7;
    const auto sx = static_cast<std::int16_t>(static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + d * 2)));
    const auto sz = static_cast<std::int16_t>(static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + d * 2 + 1)));
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
                X87Copy(p + kTo[i], Mem(Low32(draw_pool::Items()) +
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
        B(::bof3::Img(Cond_Flags_addr) + 0x21) = static_cast<unsigned char>(B(::bof3::Img(Cond_Flags_addr) + 0x21) & 0xF0);
        Gate(0xA4);
    }
    if (Cond_ByteFD == 6 && AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x27) == 0) {
        SetByte(0x92, 0x1D, 0);
        SetByte(0x93, 0x1D, 0);
        SetByte(0x92, 0x1C, 0x50);
        SetByte(0x93, 0x1C, 0x50);
    }
    if (B(::bof3::Img(AreaW1a_EntryZone_addr)) == 6) {
        if (Cond_ByteFD != 2) return;
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFFF);
        AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x27);
        B(::bof3::Img(at::kCounter0.v)) = 0;
    }
    if (B(::bof3::Img(AreaW1a_EntryZone_addr)) != 4) return;
    for (unsigned r = 0; r < at::kGridSide; ++r)
        for (unsigned c = 0; c < at::kGridSide; ++c)
            if (B(::bof3::Img(Area40_Pattern_addr) + r * 4 + c) != 0)
                AH_CALL(MoveCmd_TestFB)(static_cast<short>(at::kGridX + c), static_cast<short>(at::kGridZ + r));
}

void Area40_Inject() {
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
}
