// Group DA of the eighth round (docs/takeover-queue-round8.md, docs/worldmap_area.md):
// thirty functions the world-map and combat routes enter only through pointers.
// Each was read to its last instruction with capstone against bof3/BOF3.exe on
// 2026-09-25; every one is a faithful replacement, so no DIVERGENCE.md entry
// is owed. The start-up fuzz is worldmap_area_fuzz.cpp.
//
//   Area29_PickFieldObject    0x4037B0 (0xCB)   area 29's descriptor init (+0x40), PSX 0x801F2D2C
//   Area33_ClearCellsA        0x403CE0 (0x5F)   area 33's handler 0 (Area33_Handlers), PSX 0x801F2C38
//   Area33_ClearCellsB        0x403D40 (0x25)   area 33's handler 1, PSX 0x801F2CD0
//   WorldMap33_PlaceMessage   0x403D70 (0x87)   WorldMap_FieldHooks entry 1
//   WorldMap33_PlateRun       0x403E00 (0xD6)   world-map record 1 +0x0 (effect kind 0 on the Yraall map)
//   WorldMap33_PlateShow      0x403EE0 (0x142)  WorldMap33_PlateStates 1
//   WorldMap33_PlateGrow      0x404030 (0x41)   ... 2
//   WorldMap33_PlateHold      0x404080 (0x58)   ... 3
//   WorldMap33_PlateShrink    0x4040E0 (0x50)   ... 4
//   WorldMapHud_Run           0x404130 (0x12)   world-map record 1 +0xC (effect kind 0x58)
//   WorldMapHud_Frame         0x404150 (0xA)    WorldMapHud_States 1
//   WorldMapHud_BoxStep       0x404230 (0x12)   WorldMapHud_Frame's tail jump
//   WorldMapHud_BoxSlideIn    0x404250 (0x62)   WorldMapHud_BoxStates 1
//   WorldMapHud_BoxHold       0x4042C0 (0x6E)   ... 2
//   WorldMapHud_BoxSlideOut   0x404330 (0x57)   ... 3
//   WorldMap33_DrawDrift      0x4048E0 (0x462)  world-map record 1 +0x10
//   WorldMap_FrameWait        0x411310 (0x27)   WorldMap_FrameStep's state 0, shared by eleven tables
//   WorldMapHud_BoxWait       0x414BB0 (0x38)   WorldMapHud_BoxStates 0, shared by eleven
//   WorldMapHud_Start         0x419110 (0x1D)   WorldMapHud_States 0, shared by eleven
//   WorldMap_RecordIndex      0x462A90 (0x26)   the world-map record of Game_AreaNumber
//   EffectKind00_WorldMap     0x462AE0 (0x1A)   Effect_KindHandlers 0x00
//   EffectKind58_WorldMap     0x462B40 (0x1A)   Effect_KindHandlers 0x58
//   WorldMap_RecordHook10     0x462B80 (0x1A)   EffectKind18_States 2 and 3
//   EffectKind06_Run          0x469BB0 (0x12)   Effect_KindHandlers 0x06
//   EffectKind06_Start        0x469BD0 (0xD7)   EffectKind06_States 0, PSX 0x8019B404
//   EffectKind06_Tick         0x469CB0 (0x12)   EffectKind06_States 1, PSX 0x8019B544
//   EffectKind06_Blink        0x469CD0 (0x3D)   EffectKind06_Ticks 0..2, 4, PSX 0x8019B588
//   EffectKind06_End          0x469DB0 (0x22)   EffectKind06_States 2, PSX 0x8019B7DC
//   EffectKind18_Run          0x46D830 (0x12)   Effect_KindHandlers 0x18
//   EffectKind18_Start        0x46D850 (0x32)   EffectKind18_States 0
//
// Every call goes through worldmap_area::g (worldmap_area_callees.h); the
// dispatches through .data tables read the table in place, as the originals
// do (unchecked), so the fuzz can swap the entries.
#include "game/worldmap_area.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/worldmap_area_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace worldmap_area {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    Rand,
    AreaMap_Elevation,
    Fn<void (__cdecl*)(int, int, int)>(kSetCell),
    ScriptFlags_Set40,
    Fn<void (__cdecl*)()>(kFlagsClear40),
    Msg_OpenScript,
    AreaMap_ByteAt,
    Sprite_SetAnimation,
    WorldMap_PinSprite,
    Sprite_QueueOverlay,
    Effect_Release,
    WorldMap_FrameStep,
    WorldMapHud_BoxStep,
    WorldMap_DrawHud,
    WorldMap_RecordIndex,
    Sprite_SetAnimationBank,
    Sprite_ScriptTick,
    Sprite_UpdateScreen,
    Gpu_SetPolyFT4,
    Gpu_SetShadeTex,
    Gpu_SetSemiTrans,
    As<long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*,
                       long*)>(&Gte_RotTransPers4),
    Gte_PrimDepths4_10,
    Prim_SetTexture,
    Gfx_CommitPrim,
    MapView_ItemHalfAt,
    MapView_LinkPrimAt,
};
Callees g = kOriginals;

}  // namespace worldmap_area

using namespace worldmap_area;

namespace {

using U = std::uint32_t;

// The object being run, re-read wherever the original re-reads it.
unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
// A handler read from a .data table in place, as the originals' `call` /
// `jmp [index * 4 + table]` do: the index is not checked.
Handler Entry(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(address))))); }

// The region box's leave test the HUD machine's states share (0x404274 ..
// 0x404297 and its copies): the map's mode byte set while the object's +0xB
// is, or Field_Request 2, or bit 8 of Field_ScriptFlags (read as a dword,
// `test ch, 1`).
bool BoxLeaves(const unsigned char* o) {
    if (At(at::kMapMode)[0] != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

}  // namespace

// ===========================================================================
// Area 29 and area 33's handlers
// ===========================================================================

// original 0x4037B0 (area 29's descriptor init, +0x40 of 0x5EE270, called by
// Area_Enter; PSX 0x801F2D2C, the sibling's names/area_records.toml): a five-
// byte `jmp 0x4037C0`, then the body. Rand() & 0x3F picks one of eight field
// objects by the chances at 0x5EE268 (16 12 12 8 8 4 2 2: they sum to 64, so
// the "none" index 8 is not reached with this table); the other seven of the
// first eight Sprite_Objects records (0xA4 bytes) get byte +0 = 0. The kept
// one is put at one of eight cells, Rand() & 7 into the (x, z) byte pairs at
// 0x5EE258: +0x34 = x << 16, +0x38 = z << 16, and +0x3E the word of
// AreaMap_Elevation(+0x34 as stored, z << 16). Last, whether or not one was
// kept, Field_EdgeBits = the low word of (the leader's dword +0x134) - 5.
//
// As the original has it: the roll and the subtraction are byte-wide; the
// second Rand's low byte is taken (`mov [esp + 4], al`, then & 0xFF).
extern "C" void __cdecl Area29_PickFieldObject(void) {
    unsigned roll = static_cast<unsigned>(g.rand()) & 0x3F;
    unsigned keep = 0;
    for (; keep < 8; ++keep) {
        const unsigned chance = At(at::kArea29Weights)[keep];
        if (roll < chance) break;
        roll = (roll - chance) & 0xFF;
    }
    for (unsigned i = 0; at::kFieldObjects + i * at::kFieldObjectSize < at::kFieldObjectsEnd; ++i)
        if (i != keep) At(at::kFieldObjects + i * at::kFieldObjectSize)[0] = 0;
    if (keep < 8) {
        const unsigned cell = static_cast<unsigned>(g.rand()) & 7;
        unsigned char* const o = At(at::kFieldObjects + keep * at::kFieldObjectSize);
        const U x = static_cast<U>(At(at::kArea29Cells)[cell * 2]) << 16;
        const U z = static_cast<U>(At(at::kArea29Cells)[cell * 2 + 1]) << 16;
        SetLong(o + 0x34, static_cast<std::int32_t>(x));
        SetLong(o + 0x38, static_cast<std::int32_t>(z));
        const long height = g.elevation(Long(o + 0x34), static_cast<long>(z));
        SetWord(o + 0x3E, static_cast<unsigned>(height));
    }
    Field_EdgeBits = static_cast<unsigned short>(Long(At(at::kLeaderEdge)) - 5);
}

// original 0x403CE0 (area 33's handler 0, Area33_Handlers 0x5EF4C8 - the
// descriptor 0x5EF4D8's +0x3C; called by field-script ops 0x03 / 0xDE from
// 0x577B80; PSX 0x801F2C38): eight map cells to 0 through 0x579F00 (x, z,
// value) - x 0x3A and 0x3B of rows 0x1C, 0x1D, 0x1F and 0x20, in that order.
extern "C" void __cdecl Area33_ClearCellsA(void) {
    static constexpr unsigned char kRows[] = {0x1C, 0x1D, 0x1F, 0x20};
    for (const unsigned char z : kRows) {
        g.set_cell(0x3A, z, 0);
        g.set_cell(0x3B, z, 0);
    }
}

// original 0x403D40 (area 33's handler 1; PSX 0x801F2CD0): the cells (0x11,
// 0x15), (0x11, 0x16) and (0x12, 0x16) to 0.
extern "C" void __cdecl Area33_ClearCellsB(void) {
    g.set_cell(0x11, 0x15, 0);
    g.set_cell(0x11, 0x16, 0);
    g.set_cell(0x12, 0x16, 0);
}

// original 0x403D70 (WorldMap_FieldHooks 0x662DF0 entry 1 - 0x56DE30 calls
// entry WorldMap_RecordIndex(); the map field frame's hook for the Yraall
// map): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the row of the six 0x20-byte rows at 0x5EF51C whose
//      first word is the place 0x937F82 (6 when none); message = the word at
//      0x5EF51C + (row * 16 + (s8) Cond_ByteFA) * 2; Msg_OpenScript of it;
//      0x9039F4 (read again) + 1, Field_Request = 2, then the state stored.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 (0x57C7A0) and the
//      three bytes 0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original has it: neither the row nor
// the byte is checked (row 6 reads the state table after the rows).
extern "C" void __cdecl WorldMap33_PlaceMessage(void) {
    const auto state = static_cast<signed char>(At(at::kMsgState)[0]);
    if (state == 0) {
        g.flags_set40();
        const unsigned place = Word(At(at::kPlace));
        std::int32_t row = 0;
        for (U a = at::kPlaceMessages; a < at::kPlaceMessagesEnd; a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + static_cast<signed char>(Cond_ByteFA);
        const unsigned message = Word(At(at::kPlaceMessages + static_cast<U>(index * 2)));
        g.open_script(static_cast<unsigned short>(message));
        const auto next = static_cast<unsigned char>(At(at::kMsgState)[0] + 1);
        Field_Request = 2;
        At(at::kMsgState)[0] = next;
    } else if (state == 1) {
        if (Field_Request == 2) return;
        g.flags_clear40();
        At(at::kMsgByteBefore)[0] = 0;
        At(at::kMsgState)[0] = 0;
        At(at::kMsgByteAfter)[0] = 0;
    }
}

// ===========================================================================
// The place plate (effect kind 0 on the Yraall map, world-map record 1 +0)
// ===========================================================================

// original 0x403E00: the cell ahead of the leader - x = the high word of
// (leader +9) * (+0xC) + (+0x34), z the same of +0x10 and +0x38 (dword
// products, u8 count) - to the object's +0xC / +0x10 (sign-extended), then
// its kind at +0xB: AreaMap_ByteAt(x, z) 0xA1 -> 1, else (asked again) 0xA0
// -> 2, else (again) 0xAE -> 3, else Field_ScriptFlags2 bit 12 -> 4, else 0.
// Then WorldMap33_PlateStates 0x5EF5DC by +1: 0x401DE0 (area 16's copy's
// state 0, shared), WorldMap33_PlateShow, _Grow, _Hold, _Shrink.
//
// As the original has it: the words pushed to AreaMap_ByteAt carry a stale
// stack word above them (it reads the low words); the cell is asked for up
// to three times.
extern "C" void __cdecl WorldMap33_PlateRun(void) {
    const U steps = At(at::kLeaderSteps)[0];
    const U xs = steps * static_cast<U>(Long(At(at::kLeaderDirX))) + static_cast<U>(Long(At(at::kLeaderX)));
    const U zs = steps * static_cast<U>(Long(At(at::kLeaderDirZ))) + static_cast<U>(Long(At(at::kLeaderZ)));
    const auto x = static_cast<short>(xs >> 16);
    const auto z = static_cast<short>(zs >> 16);
    SetLong(Cur() + 0xC, x);
    SetLong(Cur() + 0x10, z);
    unsigned char kind;
    if (g.byte_at(x, z) == 0xA1) kind = 1;
    else if (g.byte_at(x, z) == 0xA0) kind = 2;
    else if (g.byte_at(x, z) == 0xAE) kind = 3;
    else if ((Field_ScriptFlags2 & 0x1000) != 0) kind = 4;
    else kind = 0;
    Cur()[0xB] = kind;
    Entry(at::kPlateStates + Cur()[1] * 4u)();
}

// original 0x403EE0 (WorldMap33_PlateStates 1): +7 = +0xB (the kind shown).
// Kind 1: the entry of WorldMap33_PlateAnims 0x5EF320 whose word is the place
// 0x937F82 - searched with NO bound, as the original has it (the six entries'
// places are 0x17, 8, 7, 0xE, 0x13, 0x60) - +0x18 = the place (zero-
// extended), and its animation byte. Kinds 2, 3, 4: animations 3, 0, 1. For
// each of the four: +0x40 = 0, +0x44 = 0x10000, +0x48 = 2, +9 = 8,
// Sprite_SetAnimation(the animation), +1 = 2 (the grow). Kind 0 and any other
// do nothing more.
extern "C" void __cdecl WorldMap33_PlateShow(void) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = Word(At(at::kPlace));
        U entry = at::kPlateAnims;
        while (Word(At(entry)) != place) entry += 4;
        SetLong(Cur() + 0x18, static_cast<std::int32_t>(place));
        animation = At(entry + 2)[0];
    } else if (kind == 2) {
        animation = 3;
    } else if (kind == 3) {
        animation = 0;
    } else if (kind == 4) {
        animation = 1;
    } else {
        return;
    }
    SetLong(Cur() + 0x40, 0);
    SetLong(Cur() + 0x44, 0x10000);
    Cur()[0x48] = 2;
    Cur()[9] = 8;
    g.set_animation(static_cast<unsigned char>(animation));
    Cur()[1] = 2;
}

// original 0x404030 (WorldMap33_PlateStates 2): WorldMap_PinSprite; +0x40 +=
// 0x2000; +9 - 1, and at 0: +0x48 = 0, +1 = 3 (the hold); then a tail jump to
// Sprite_QueueOverlay. Eight frames from 0 to 0x10000.
extern "C" void __cdecl WorldMap33_PlateGrow(void) {
    g.pin_sprite();
    SetLong(Cur() + 0x40, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x40)) + 0x2000));
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] == 0) {
        o[0x48] = 0;
        Cur()[1] = 3;
    }
    g.queue_overlay();
}

// original 0x404080 (WorldMap33_PlateStates 3): WorldMap_PinSprite; unless
// Game_Mode is 1, the plate leaves (+0x48 = 2, +9 = 8, +1 = 4: the shrink)
// when the kind +0xB is not the one shown +7, or Field_Request is 5, or the
// kind shown is 1 and +0x18 is not the place 0x937F82 (zero-extended); then a
// tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl WorldMap33_PlateHold(void) {
    g.pin_sprite();
    if (Game_Mode != 1) {
        unsigned char* const o = Cur();
        const unsigned shown = o[7];
        const bool leaves = o[0xB] != shown || Field_Request == 5 ||
                            (shown == 1 && static_cast<U>(Long(o + 0x18)) != Word(At(at::kPlace)));
        if (leaves) {
            o[0x48] = 2;
            Cur()[9] = 8;
            Cur()[1] = 4;
        }
    }
    g.queue_overlay();
}

// original 0x4040E0 (WorldMap33_PlateStates 4): WorldMap_PinSprite; +0x40 -=
// 0x2000; +9 - 1. Not yet 0: a tail jump to Sprite_QueueOverlay. At 0: with
// Field_Request 5 a tail jump to Effect_Release, else +0x48 = 0 and +1 = 1
// (the show again), with no overlay that frame.
extern "C" void __cdecl WorldMap33_PlateShrink(void) {
    g.pin_sprite();
    SetLong(Cur() + 0x40, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x40)) - 0x2000));
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] != 0) {
        g.queue_overlay();
        return;
    }
    if (Field_Request == 5) {
        g.effect_release();
        return;
    }
    o[0x48] = 0;
    Cur()[1] = 1;
}

// ===========================================================================
// The map's HUD task (effect kind 0x58 on the Yraall map, record 1 +0xC)
// ===========================================================================

// original 0x404130: WorldMapHud_States 0x5EF5F0 by +1 - WorldMapHud_Start
// (shared), WorldMapHud_Frame.
extern "C" void __cdecl WorldMapHud_Run(void) { Entry(at::kHudStates + Cur()[1] * 4u)(); }

// original 0x419110 (WorldMapHud_States 0; the same entry in the eleven
// world-map copies' tables): +9 = 0, +0xB = 0, +1 + 1.
extern "C" void __cdecl WorldMapHud_Start(void) {
    Cur()[9] = 0;
    Cur()[0xB] = 0;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
}

// original 0x404150 (WorldMapHud_States 1): `call 0x404160; jmp 0x404230` -
// WorldMap_FrameStep (the dial's slide, byte +2), then WorldMapHud_BoxStep
// (the region box's, byte +3).
extern "C" void __cdecl WorldMapHud_Frame(void) {
    g.frame_step();
    g.box_step();
}

// original 0x411310 (WorldMap_FrameStep's state 0, table 0x5EF5F8; the same
// entry in eleven tables): unless the map's mode byte 0x9045FA is 2 or
// Field_ScriptFlags bit 8 is set, the frame's y +0x2E = -0x30 and +2 + 1.
extern "C" void __cdecl WorldMap_FrameWait(void) {
    if (At(at::kMapMode)[0] == 2) return;
    if ((Field_ScriptFlags & 0x100) != 0) return;
    SetWord(Cur() + 0x2E, 0xFFD0);
    Cur()[2] = static_cast<unsigned char>(Cur()[2] + 1);
}

// original 0x404230: WorldMapHud_BoxStates 0x5EF608 by +3 - WorldMapHud_BoxWait
// (shared), _BoxSlideIn, _BoxHold, _BoxSlideOut.
extern "C" void __cdecl WorldMapHud_BoxStep(void) { Entry(at::kBoxStates + Cur()[3] * 4u)(); }

// original 0x414BB0 (WorldMapHud_BoxStates 0; the same entry in eleven
// tables): unless the box leaves (BoxLeaves above), its y +0x30 = 0xF0 and +3
// + 1. Nothing is drawn.
extern "C" void __cdecl WorldMapHud_BoxWait(void) {
    unsigned char* const o = Cur();
    if (BoxLeaves(o)) return;
    SetWord(o + 0x30, 0xF0);
    Cur()[3] = static_cast<unsigned char>(Cur()[3] + 1);
}

// original 0x404250 (WorldMapHud_BoxStates 1): y +0x30 -= 10; at 0xC8 and
// below (s16) +3 + 1; then when the box leaves +3 = 3 (over the + 1);
// WorldMap_DrawHud(0x5C, y).
//
// As the original has it: the y pushed is the word in a register whose upper
// half is the object pointer's; WorldMap_DrawHud and what it calls read the
// low word (docs/world-map-hud.md section 1.4), so ours passes the word
// sign-extended.
extern "C" void __cdecl WorldMapHud_BoxSlideIn(void) {
    SetWord(Cur() + 0x30, Word(Cur() + 0x30) - 10u);
    unsigned char* o = Cur();
    if (S16(o + 0x30) <= 0xC8) {
        o[3] = static_cast<unsigned char>(o[3] + 1);
        o = Cur();
    }
    if (BoxLeaves(o)) {
        o[3] = 3;
        o = Cur();
    }
    g.draw_hud(0x5C, S16(o + 0x30));
}

// original 0x4042C0 (WorldMapHud_BoxStates 2): while +0xB is 0 the counter
// +9 + 1, and at 0x5A and above (u8) +0xB + 1; then when the box leaves +3 +
// 1; WorldMap_DrawHud(0x5C, y).
extern "C" void __cdecl WorldMapHud_BoxHold(void) {
    unsigned char* o = Cur();
    if (o[0xB] == 0) {
        o[9] = static_cast<unsigned char>(o[9] + 1);
        o = Cur();
        if (o[9] >= 0x5A) {
            o[0xB] = static_cast<unsigned char>(o[0xB] + 1);
            o = Cur();
        }
    }
    if (BoxLeaves(o)) {
        o[3] = static_cast<unsigned char>(o[3] + 1);
        o = Cur();
    }
    g.draw_hud(0x5C, S16(o + 0x30));
}

// original 0x404330 (WorldMapHud_BoxStates 3): y +0x30 += 10; at 0xF0 and
// above (s16) +3 = 0; then unless the mode byte is set (whatever +0xB), or
// Field_Request is 2, or Field_ScriptFlags bit 8, +3 = 1 (over the 0);
// WorldMap_DrawHud(0x5C, y).
extern "C" void __cdecl WorldMapHud_BoxSlideOut(void) {
    SetWord(Cur() + 0x30, Word(Cur() + 0x30) + 10u);
    unsigned char* o = Cur();
    if (S16(o + 0x30) >= 0xF0) {
        o[3] = 0;
        o = Cur();
    }
    if (At(at::kMapMode)[0] == 0 && Field_Request != 2 && (Field_ScriptFlags & 0x100) == 0) {
        o[3] = 1;
        o = Cur();
    }
    g.draw_hud(0x5C, S16(o + 0x30));
}

// ===========================================================================
// The drift layer (world-map record 1 +0x10, through WorldMap_RecordHook10)
// ===========================================================================

// original 0x4048E0. Once (+2 == 0): +0x3A += Frame_Counter & 0xF (a word),
// +2 + 1. Nothing more unless Draw_PassFlags bit 2. With b = +0xB (u8) and e
// = b - 2:
//   - the dword +0x38 += b << 10; the word +0x3A = -8 when (s16) above the
//     map's height AreaMap_Header[1] + 8;
//   - nothing more unless |leader cell x - (s16) +0x36| <= 25 OR |leader cell
//     z - (s16) +0x3A| <= 25 (the high words of Field_Kind2X / Z);
//   - one POLY_FT4 at Gfx_PacketNext: Gpu_SetPolyFT4, Gpu_SetShadeTex(0); the
//     square of half-side r = |15 - (Frame_Counter & 0x1F)| + ((4 - b) << 8)
//     about ((+0x34 >> 9) - 0x3FC0, (+0x38 >> 9) - 0x3FC0), its four
//     SVECTORs (x, z, -0x300) into Prim_VertexScratch (pads untouched),
//     Gte_RotTransPers4 into the corners with two locals (ten arguments),
//     Gte_PrimDepths4_10, Prim_SetTexture(e | 0xBB509100, prim, 1),
//     Gfx_CommitPrim(4, 0x48);
//   - then (17 - 4b) rows j of (16 - 4b) columns i: for each map item
//     MapView_ItemHalfAt(+0x36 + i + b - 4, +0x3A + j + b - 4) answers, a
//     semi-transparent POLY_FT4 at Gfx_PacketNext (read again) with the
//     item's four corners, colour 0x28, CLUT 0x78CB, tpage 0x5B, and u / v
//     bytes from WorldMap33_DriftUV by e (u base, v base, cell size m): u = i
//     * m + u base (i + 1 for corners 1, 3), v = j * m - ((u16 +0x38 * m) >>
//     16) + v base (j + 1 for corners 2, 3); MapView_LinkPrimAt(x << 16, z
//     << 16, 1, 0x48) with the item's cell read again.
//
// As the original has it: e indexes the u / v bytes unchecked (b 2 and 3 are
// the table's; b 0, 1 or 4 read its neighbours); the byte products wrap; the
// distance test is an OR (docs/worldmap_area.md section 4).
extern "C" void __cdecl WorldMap33_DrawDrift(void) {
    unsigned char* o = Cur();
    if (o[2] == 0) {
        SetWord(o + 0x3A, Word(o + 0x3A) + (Frame_Counter & 0xF));
        Cur()[2] = static_cast<unsigned char>(Cur()[2] + 1);
        o = Cur();
    }
    if ((Draw_PassFlags & 4) == 0) return;
    const std::int32_t b = o[0xB];
    const std::int32_t e = b - 2;
    SetLong(o + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x38)) + (static_cast<U>(b) << 10)));
    o = Cur();
    if (S16(o + 0x3A) > static_cast<std::int32_t>(At(at::kMapHeight)[0]) + 8) {
        SetWord(o + 0x3A, 0xFFF8);
        o = Cur();
    }
    std::int32_t dx = S16(At(at::kLeaderCellX)) - S16(o + 0x36);
    if (dx < 0) dx = -dx;
    if (dx > 0x19) {
        std::int32_t dz = S16(At(at::kLeaderCellZ)) - S16(o + 0x3A);
        if (dz < 0) dz = -dz;
        if (dz > 0x19) return;
    }

    unsigned char* const prim = Gfx_PacketNext;
    g.set_poly_ft4(prim);
    g.set_shade_tex(prim, 0);
    o = Cur();
    const std::int32_t side = 2 - e;   // 4 - b
    std::int32_t r = 15 - static_cast<std::int32_t>(Frame_Counter & 0x1F);
    if (r < 0) r = -r;
    r += static_cast<std::int32_t>(static_cast<U>(side) << 8);
    const std::int32_t cx = (Long(o + 0x34) >> 9) - 0x3FC0;
    const std::int32_t cz = (Long(o + 0x38) >> 9) - 0x3FC0;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    SetWord(v + 4, 0xFD00);
    SetWord(v + 0x12, static_cast<unsigned>(cz + r));
    SetWord(v + 0x1A, static_cast<unsigned>(cz + r));
    SetWord(v + 0x08, static_cast<unsigned>(cx + r));
    SetWord(v + 0x18, static_cast<unsigned>(cx + r));
    SetWord(v + 0x00, static_cast<unsigned>(cx - r));
    SetWord(v + 0x10, static_cast<unsigned>(cx - r));
    SetWord(v + 0x02, static_cast<unsigned>(cz - r));
    SetWord(v + 0x0A, static_cast<unsigned>(cz - r));
    SetWord(v + 0x0C, 0xFD00);
    SetWord(v + 0x14, 0xFD00);
    SetWord(v + 0x1C, 0xFD00);
    long depth = 0, flag = 0;
    const auto* const sv = reinterpret_cast<const short*>(v);
    g.rot_trans_pers4(sv, sv + 4, sv + 8, sv + 12, reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                      reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
    g.prim_depths(prim);
    g.set_texture(static_cast<U>(e) | 0xBB509100u, prim, 1);
    g.commit_prim(4, 0x48);

    const std::int32_t rows = 9 - 4 * e;   // 17 - 4b
    const std::int32_t columns = 4 * side; // 16 - 4b
    for (std::int32_t j = 0; j < rows; ++j) {
        for (std::int32_t i = 0; i < columns; ++i) {
            o = Cur();
            unsigned char* const item = g.item_half_at(S16(o + 0x36) + i + e - 2, S16(o + 0x3A) + j + e - 2);
            if (item == nullptr) continue;
            unsigned char* const q = Gfx_PacketNext;
            g.set_poly_ft4(q);
            g.set_shade_tex(q, 0);
            g.set_semi_trans(q, 1);
            std::memcpy(q + 0x08, item + 0x08, 12);
            std::memcpy(q + 0x18, item + 0x18, 12);
            std::memcpy(q + 0x28, item + 0x28, 12);
            std::memcpy(q + 0x38, item + 0x38, 12);
            SetWord(q + 0x16, 0x78CB);
            SetWord(q + 0x26, 0x5B);
            q[4] = q[5] = q[6] = 0x28;
            const U m = At(at::kDriftSize + static_cast<U>(e))[0];
            const U ubase = At(at::kDriftUBase + static_cast<U>(e))[0];
            const U vbase = At(at::kDriftVBase + static_cast<U>(e))[0];
            const U scroll = (Word(Cur() + 0x38) * m) >> 16;
            const U ui = static_cast<U>(i), uj = static_cast<U>(j);
            q[0x14] = static_cast<unsigned char>(ui * m + ubase);
            q[0x15] = static_cast<unsigned char>(uj * m - scroll + vbase);
            q[0x24] = static_cast<unsigned char>((ui + 1) * m + ubase);
            q[0x25] = static_cast<unsigned char>(uj * m - scroll + vbase);
            q[0x34] = static_cast<unsigned char>(ui * m + ubase);
            q[0x35] = static_cast<unsigned char>((uj + 1) * m - scroll + vbase);
            q[0x44] = static_cast<unsigned char>((ui + 1) * m + ubase);
            q[0x45] = static_cast<unsigned char>((uj + 1) * m - scroll + vbase);
            o = Cur();
            const U x = static_cast<U>(S16(o + 0x36) + i + e - 2) << 16;
            const U z = static_cast<U>(S16(o + 0x3A) + j + e - 2) << 16;
            g.link_prim_at(x, z, 1, 0x48);
        }
    }
}

// ===========================================================================
// The world-map records and the effect kinds that go through them
// ===========================================================================

// original 0x462A90 (4,560 calls on the world-map route): the index of the
// first of the eleven 0x1C-byte WorldMap_Records whose area byte +0x18 is
// Game_AreaNumber (u16 against the zero-extended byte), or 11 when none -
// the whole eax. The records are the eleven world maps (areas 16, 33, 45, 65,
// 87, 88, 104, 115, 121, 151, 152), each with five code pointers and a data
// pointer; the callers take the index's low byte.
extern "C" unsigned __cdecl WorldMap_RecordIndex(void) {
    const unsigned area = Game_AreaNumber;
    unsigned index = 0;
    for (U a = at::kRecordAreas; a < at::kRecordAreasEnd; a += at::kRecordSize, ++index)
        if (At(a)[0] == area) break;
    return index;
}

namespace {
// A world-map record's code pointer at `slot`, by WorldMap_RecordIndex's low
// byte, read in place. Index 11 (no world map) reads the dwords after the
// eleventh record - the table 0x653A44 of 0x462BA0 (0x462BC0, 0x462BF0,
// 0x462E70, 0x462EB0, 0x462FC0, ...) - as the original does; record 6 (area
// 104) holds nulls at +4, +8 and +0x14.
Handler RecordEntry(U slot) {
    const U index = g.record_index() & 0xFF;
    return Entry(at::kRecords + index * at::kRecordSize + slot);
}
}  // namespace

// original 0x462AE0 (Effect_KindHandlers 0x00): a tail jump to the world-map
// record's +0 (WorldMap33_PlateRun on the Yraall map).
extern "C" void __cdecl EffectKind00_WorldMap(void) { RecordEntry(0)(); }

// original 0x462B40 (Effect_KindHandlers 0x58): the record's +0xC
// (WorldMapHud_Run on the Yraall map).
extern "C" void __cdecl EffectKind58_WorldMap(void) { RecordEntry(0xC)(); }

// original 0x462B80 (EffectKind18_States 2 and 3): the record's +0x10
// (WorldMap33_DrawDrift on the Yraall map).
extern "C" void __cdecl WorldMap_RecordHook10(void) { RecordEntry(0x10)(); }

// ===========================================================================
// Effect kind 6 (the combat route's) and kind 0x18
// ===========================================================================

// original 0x469BB0 (Effect_KindHandlers 0x06): EffectKind06_States 0x653EC8
// by +1 - _Start, _Tick, _End.
extern "C" void __cdecl EffectKind06_Run(void) { Entry(at::kKind06States + Cur()[1] * 4u)(); }

// original 0x469BD0 (EffectKind06_States 0 and 0x653EF4's entry 0; PSX
// 0x8019B404): Sprite_SetAnimationBank(0x18); +0x48 = 0; the word +0x2E +=
// the word +0xC, +0x30 -= the word +0x10; +0x24 = 0, +0x2A = 0, +0x29 = 2.
// Variant +6 == 3: byte +0 |= 0x20, +0x5C = 1, +0x5F / +0x5E / +0x5D = 0xF8,
// +9 = 0x14; any other: +0x5C and the three = 0, +9 = 0x10. Then
// Sprite_SetAnimation(EffectKind06_Anims[+6] - 0 0 2 1 3 4, unchecked) and
// +1 = 1.
extern "C" void __cdecl EffectKind06_Start(void) {
    g.set_bank(0x18);
    Cur()[0x48] = 0;
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) + Word(Cur() + 0xC));
    SetWord(Cur() + 0x30, Word(Cur() + 0x30) - Word(Cur() + 0x10));
    Cur()[0x24] = 0;
    Cur()[0x2A] = 0;
    Cur()[0x29] = 2;
    unsigned char* const o = Cur();
    if (o[6] == 3) {
        o[0] = static_cast<unsigned char>(o[0] | 0x20);
        Cur()[0x5C] = 1;
        Cur()[0x5F] = 0xF8;
        Cur()[0x5E] = 0xF8;
        Cur()[0x5D] = 0xF8;
        Cur()[9] = 0x14;
    } else {
        o[0x5C] = 0;
        Cur()[0x5F] = 0;
        Cur()[0x5E] = 0;
        Cur()[0x5D] = 0;
        Cur()[9] = 0x10;
    }
    g.set_animation(At(at::kKind06Anims + Cur()[6])[0]);
    Cur()[1] = 1;
}

// original 0x469CB0 (EffectKind06_States 1; PSX 0x8019B544): EffectKind06_Ticks
// 0x653EDC by the variant +6 - _Blink for 0, 1, 2 and 4, 0x469D40 for 3,
// 0x469D10 for 5 (neither in this round's queue).
extern "C" void __cdecl EffectKind06_Tick(void) { Entry(at::kKind06Ticks + Cur()[6] * 4u)(); }

// original 0x469CD0 (PSX 0x8019B588): +9 - 1; at 0 +1 + 1 and nothing more.
// Otherwise Sprite_ScriptTick, and - only while bit 2 of +9 is set, so it
// blinks - a tail jump to Sprite_QueueOverlay when the kind +5 is 6, else to
// Sprite_UpdateScreen. Sprite_ScriptTick's answer is not read.
extern "C" void __cdecl EffectKind06_Blink(void) {
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* o = Cur();
    if (o[9] == 0) {
        o[1] = static_cast<unsigned char>(o[1] + 1);
        return;
    }
    g.script_tick();
    o = Cur();
    if ((o[9] & 4) == 0) return;
    if (o[5] == 6) g.queue_overlay();
    else g.update_screen();
}

// original 0x469DB0 (EffectKind06_States 2; PSX 0x8019B7DC): +0x5F, +0x5E,
// +0x5D = 0, then a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind06_End(void) {
    Cur()[0x5F] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5D] = 0;
    g.effect_release();
}

// original 0x46D830 (Effect_KindHandlers 0x18): EffectKind18_States 0x65406C
// by +1.
extern "C" void __cdecl EffectKind18_Run(void) { Entry(at::kKind18States + Cur()[1] * 4u)(); }

// original 0x46D850 (EffectKind18_States 0): +1 = +0xB (the effect's sub-kind
// is its next state), +2 = +3 = +4 = +9 = 0.
extern "C" void __cdecl EffectKind18_Start(void) {
    Cur()[1] = Cur()[0xB];
    Cur()[2] = 0;
    Cur()[3] = 0;
    Cur()[4] = 0;
    Cur()[9] = 0;
}

// ===========================================================================

void WorldmapArea_Inject() {
    if (bof3::WantsShadow("worldmap_area")) worldmap_area::SelfTest();
    BOF3_INJECT(Area29_PickFieldObject);
    BOF3_INJECT(Area33_ClearCellsA);
    BOF3_INJECT(Area33_ClearCellsB);
    BOF3_INJECT(WorldMap33_PlaceMessage);
    BOF3_INJECT(WorldMap33_PlateRun);
    BOF3_INJECT(WorldMap33_PlateShow);
    BOF3_INJECT(WorldMap33_PlateGrow);
    BOF3_INJECT(WorldMap33_PlateHold);
    BOF3_INJECT(WorldMap33_PlateShrink);
    BOF3_INJECT(WorldMapHud_Run);
    BOF3_INJECT(WorldMapHud_Start);
    BOF3_INJECT(WorldMapHud_Frame);
    BOF3_INJECT(WorldMap_FrameWait);
    BOF3_INJECT(WorldMapHud_BoxStep);
    BOF3_INJECT(WorldMapHud_BoxWait);
    BOF3_INJECT(WorldMapHud_BoxSlideIn);
    BOF3_INJECT(WorldMapHud_BoxHold);
    BOF3_INJECT(WorldMapHud_BoxSlideOut);
    BOF3_INJECT(WorldMap33_DrawDrift);
    BOF3_INJECT(WorldMap_RecordIndex);
    BOF3_INJECT(EffectKind00_WorldMap);
    BOF3_INJECT(EffectKind58_WorldMap);
    BOF3_INJECT(WorldMap_RecordHook10);
    BOF3_INJECT(EffectKind06_Run);
    BOF3_INJECT(EffectKind06_Start);
    BOF3_INJECT(EffectKind06_Tick);
    BOF3_INJECT(EffectKind06_Blink);
    BOF3_INJECT(EffectKind06_End);
    BOF3_INJECT(EffectKind18_Run);
    BOF3_INJECT(EffectKind18_Start);
}
