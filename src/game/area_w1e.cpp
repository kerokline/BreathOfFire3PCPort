// World 1's areas 65 and 67: the code of the PSX's BIN/WORLD01/AREA065 and
// AREA067.EMI compiled into the exe at 0x40B8C0..0x40CEEC - 49 functions,
// each read to its last instruction with capstone (2026-09-28) and taken
// through the area harness (area_harness.h). Round ten group AR1E;
// docs/area_w1e.md has the areas one section each.
//
// Area 65 is a world map (WorldMap_Records' record 3): its code is area 45's
// (docs/area_w1b.md section 5, itself area 16's), instruction for instruction
// over its own tables, but for the plate's animation bank (0x53, area 45's
// 0x3B) and the place hook's name sets (two sets of five items over five text
// rows, area 45's three of four over four); its region label reads area 45's
// cell 0x803584. Its block also holds Area65_Record8Move, the record +8
// effect's state 2 of all ten world maps. Area 67 has three choices and
// nineteen handlers: messages, effects spawned at the party members, nudges of
// the first two field objects, a music fade, a map byte and a state reset; and
// object trigger 31 (placed in its block by address) arms a mode tail.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through area 65's .data state tables abort past the table where
// the original would call whatever the next dwords hold (the owner's rule for
// an unchecked index, round9 doc section 6; no route reaches it). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/area_w1e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1e::at;
using U = std::uint32_t;
using area_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char& B(U address) { return At(address)[0]; }
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// Area 65's own functions another of them calls directly.
constexpr U kFrameStep65 = 0x40BDF0, kFrameHold65 = 0x40BE40, kBoxStep65 = 0x40BEC0;
constexpr U kDrawFrame65 = 0x40C020, kDrawSprite65 = 0x40C1F0, kDrawHud65 = 0x40C2B0;
using DrawAt = void (__cdecl*)(int, int);
using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (At(at::kMapMode)[0] != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

// The first of `count` button-table entries whose mask word has a bit of the
// button word's low 16 bits: its sprite byte, or -1.
int KeyIndex(U button_word, U count) {
    for (U k = 0; k < count; ++k) {
        const unsigned char* const entry = At(at::kA65Buttons + k * 4);
        if ((Word(entry) & button_word & 0xFFFF) != 0) return entry[2];
    }
    return -1;
}

}  // namespace

// ===========================================================================
// Area 65: the world map's field hook (WorldMap_FieldHooks entry 3)
// ===========================================================================

// original 0x40B8C0 (WorldMap_FieldHooks 0x662DF0 entry 3; 0x56DE30 calls the
// entry of WorldMap_RecordIndex): area 45's Area45_PlaceMessage over area 65's
// tables, but for the name sets' shape. A two-state machine on the s8
// 0x9039F4.
//   0: ScriptFlags_Set40; the cell the leader stands on (the high words of its
//      +0x34 / +0x38) asked of AreaMap_ByteAt. 0xA1 (a place): the message of
//      row (the first of the eleven 0x20-byte rows at 0x604714 whose word is
//      the place 0x937F82, 11 when none) * 16 + (s8) Cond_ByteFA,
//      Msg_OpenScript. Any other: the cell's 4-byte record of 0x6043F4 whose
//      (x, z) bytes are the leader's cell words (read again after the call;
//      searched with NO bound), its id byte +3; the name set of 0x604874 - two
//      6-byte records (id, five items), area 45's are three of five bytes -
//      whose id it is (2 when none); up to FIVE Text_Records rows (area 45:
//      four) from its item bytes (0xFF ends them): "????????" and a 0 for an
//      item whose byte at 0x9040EC is 0, else the item's 16-byte name
//      (0x669CD8 for item 0x16, else Item_NamePtr(0, item + 0x38));
//      Msg_OpenScript(set + 0x16). Then 0x9039F4 (read again) + 1 and
//      Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original: neither the row, the byte
// nor the set 2 (whose items are the plate state table's first bytes) is
// checked.
extern "C" void __cdecl Area65_PlaceMessage(void) {
    const auto state = static_cast<signed char>(At(at::kMsgState)[0]);
    if (state == 1) {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        At(at::kMsgMode)[0] = 0;
        At(at::kMsgState)[0] = 0;
        At(at::kMsgArg)[0] = 0;
        return;
    }
    if (state != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    const auto z = static_cast<short>(Word(At(at::kLeaderCellWordZ)));
    const auto x = static_cast<short>(Word(At(at::kLeaderCellWordX)));
    if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xA1) {
        const unsigned place = Word(At(at::kPlace));
        std::int32_t row = 0;
        for (U a = at::kA65PlaceMessages; a < at::kA65PlaceMessagesEnd; a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(at::kA65PlaceMessages + static_cast<U>(index * 2)))));
    } else {
        const unsigned cx = Word(At(at::kLeaderCellWordX));
        const unsigned cz = Word(At(at::kLeaderCellWordZ));
        U cell = at::kA65Cells;
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = at::kA65NameSets; a < at::kA65NameSetsEnd; a += at::kA65NameSetStride, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(at::kA65NameSets + 1 + set * at::kA65NameSetStride);
        for (U row = at::kTextRecords; row < at::kTextRecordsEnd; row += 0x20, ++items) {
            const unsigned item = items[0];
            if (item == 0xFF) break;
            unsigned char* const out = At(row);
            if (At(at::kItemsHeld + item)[0] == 0) {
                SetLong(out, 0x3F3F3F3F);
                SetLong(out + 4, 0x3F3F3F3F);
                out[8] = 0;
            } else {
                const unsigned char* const name =
                    item == 0x16 ? At(at::kItemNameKey) : AH_CALL(Item_NamePtr)(0, (item + 0x38) & 0xFF);
                std::memcpy(out, name, 16);
            }
        }
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(set + 0x16));
    }
    const auto next = static_cast<unsigned char>(At(at::kMsgState)[0] + 1);
    Field_Request = 2;
    At(at::kMsgState)[0] = next;
}

// ===========================================================================
// Area 65: the place plate (world-map record 3 +0: effect kind 0)
// ===========================================================================

// original 0x40BA40 (WorldMap_Records[3] +0; area 45's 0x407CC0): the cell
// ahead of the leader - the high words of (leader +9) * (+0xC) + (+0x34) and
// of the same with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the kind
// +0xB by AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2, 0xAE
// 3, else Field_ScriptFlags2 bit 12 4, else 0; then Area65_PlateStates
// 0x604880 by +1 (read after the calls; ours aborts past its five entries).
extern "C" void __cdecl Area65_PlateRun(void) {
    const U steps = At(at::kLeaderSteps)[0];
    const U zs = steps * static_cast<U>(Long(At(at::kLeaderDirZ))) + static_cast<U>(Long(At(at::kLeaderZ)));
    const U xs = steps * static_cast<U>(Long(At(at::kLeaderDirX))) + static_cast<U>(Long(At(at::kLeaderX)));
    const auto x = static_cast<short>(xs >> 16);
    const auto z = static_cast<short>(zs >> 16);
    SetLong(Cur() + 0xC, x);
    SetLong(Cur() + 0x10, z);
    unsigned char kind;
    if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xA1) kind = 1;
    else if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xA0) kind = 2;
    else if (AH_CALL(AreaMap_ByteAt)(x, z) == 0xAE) kind = 3;
    else if ((Field_ScriptFlags2 & 0x1000) != 0) kind = 4;
    else kind = 0;
    Cur()[0xB] = kind;
    StateEntry("Area65_PlateRun", at::kA65PlateStates, 5, Cur()[1])();
}

// original 0x40BB20 (Area65_PlateStates 0; area 45's 0x407DA0 but for the
// bank, 0x53 where area 45 pushes 0x3B and area 16 0xF): +0x24 = 0x80,
// Sprite_SetAnimationBank(0x53), then +0x29 = 5, +0x2A, +0x5D, +0x5E, +0x5F =
// 0 and +1 = 1 (the show), Sprite_Current read again for each store.
extern "C" void __cdecl Area65_PlateStart(void) {
    Cur()[0x24] = 0x80;
    AH_CALL(Sprite_SetAnimationBank)(0x53);
    Cur()[0x29] = 5;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    Cur()[1] = 1;
}

// original 0x40BB70 (Area65_PlateStates 1; area 45's 0x407DF0): +7 = +0xB.
// Kind 1: the entry of Area65_PlateAnims 0x6043C8 whose word is the place
// 0x937F82 - searched with NO bound - +0x18 = the place (zero-extended), its
// animation byte. Kinds 2, 3, 4: animations 3, 0, 1. Each: +0x40 = 0, +0x44 =
// 0x10000, +0x48 = 2, +9 = 8, Sprite_SetAnimation, +1 = 2. Other kinds: no more.
extern "C" void __cdecl Area65_PlateShow(void) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = Word(At(at::kPlace));
        U entry = at::kA65PlateAnims;
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
    AH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Cur()[1] = 2;
}

// original 0x40BCC0 (Area65_PlateStates 2; area 45's 0x407F40):
// WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0 +0x48 = 0 and +1 = 3;
// then a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area65_PlateGrow(void) {
    AH_CALL(WorldMap_PinSprite)();
    SetLong(Cur() + 0x40, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x40)) + 0x2000));
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] == 0) {
        o[0x48] = 0;
        Cur()[1] = 3;
    }
    AH_CALL(Sprite_QueueOverlay)();
}

// original 0x40BD10 (Area65_PlateStates 3; area 45's 0x407F90):
// WorldMap_PinSprite; unless Game_Mode is 1 the plate leaves (+0x48 = 2, +9 =
// 8, +1 = 4) when +0xB is not +7, or Field_Request is 5, or +7 is 1 and +0x18
// is not the place (zero-extended); a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area65_PlateHold(void) {
    AH_CALL(WorldMap_PinSprite)();
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
    AH_CALL(Sprite_QueueOverlay)();
}

// original 0x40BD70 (Area65_PlateStates 4; area 45's 0x407FF0):
// WorldMap_PinSprite; +0x40 -= 0x2000; +9 - 1. Not 0: a tail jump to
// Sprite_QueueOverlay. At 0: Field_Request 5 a tail jump to Effect_Release,
// else +0x48 = 0 and +1 = 1.
extern "C" void __cdecl Area65_PlateShrink(void) {
    AH_CALL(WorldMap_PinSprite)();
    SetLong(Cur() + 0x40, static_cast<std::int32_t>(static_cast<U>(Long(Cur() + 0x40)) - 0x2000));
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* const o = Cur();
    if (o[9] != 0) {
        AH_CALL(Sprite_QueueOverlay)();
        return;
    }
    if (Field_Request == 5) {
        AH_CALL(Effect_Release)();
        return;
    }
    o[0x48] = 0;
    Cur()[1] = 1;
}

// ===========================================================================
// Area 65: the HUD task (record 3 +0xC: effect kind 0x58), the dial frame's
// slide and the region box
// ===========================================================================

// original 0x40BDC0 (record 3 +0xC; area 45's 0x408040): Area65_HudStates
// 0x604894 by +1 - WorldMapHud_Start (shared), Area65_HudFrame.
extern "C" void __cdecl Area65_HudRun(void) { StateEntry("Area65_HudRun", at::kA65HudStates, 2, Cur()[1])(); }

// original 0x40BDE0 (Area65_HudStates 1; area 45's 0x408060): `call 0x40BDF0;
// jmp 0x40BEC0` - the frame's slide, then the region box's.
extern "C" void __cdecl Area65_HudFrame(void) {
    area_harness::Phase(kFrameStep65)();
    area_harness::Phase(kBoxStep65)();
}

// original 0x40BDF0 (area 45's 0x408070): Area65_FrameStates 0x60489C by +2 -
// WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold, _FrameSlideOut. A
// tail jump; ours aborts past the four entries.
extern "C" void __cdecl Area65_FrameStep(void) { StateEntry("Area65_FrameStep", at::kA65FrameStates, 4, Cur()[2])(); }

// original 0x40BE10 (Area65_FrameStates 1; area 45's 0x408090): the word
// +0x2E += 0x10; at 0x10 and above (s16) +2 + 1; then a tail jump to
// Area65_FrameHold.
extern "C" void __cdecl Area65_FrameSlideIn(void) {
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) + 0x10u);
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(kFrameHold65)();
}

// original 0x40BE40 (Area65_FrameStates 2; area 45's 0x4080C0): the mode byte
// 2 makes +2 = 3; Area65_DrawFrame(0x10, the word +0x2E). The y pushed
// carries a stale high half the drawing never reads: ours passes the word
// sign-extended.
extern "C" void __cdecl Area65_FrameHold(void) {
    if (At(at::kMapMode)[0] == 2) Cur()[2] = 3;
    AH_AT(DrawAt, kDrawFrame65)(0x10, S16(Cur() + 0x2E));
}

// original 0x40BE70 (Area65_FrameStates 3; area 45's 0x4080F0): the word
// +0x2E -= 0x10; at -0x30 and below +2 = 0; unless the mode byte is 2, +2 = 1
// (over the 0); Area65_DrawFrame(0x10, y).
extern "C" void __cdecl Area65_FrameSlideOut(void) {
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) - 0x10u);
    unsigned char* o = Cur();
    if (S16(o + 0x2E) <= -0x30) {
        o[2] = 0;
        o = Cur();
    }
    if (At(at::kMapMode)[0] != 2) {
        o[2] = 1;
        o = Cur();
    }
    AH_AT(DrawAt, kDrawFrame65)(0x10, S16(o + 0x2E));
}

// original 0x40BEC0 (area 45's 0x408140): Area65_BoxStates 0x6048AC by +3 -
// WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold, _BoxSlideOut.
extern "C" void __cdecl Area65_BoxStep(void) { StateEntry("Area65_BoxStep", at::kA65BoxStates, 4, Cur()[3])(); }

// original 0x40BEE0 (Area65_BoxStates 1; area 45's 0x408160): y +0x30 -= 10;
// at 0xC8 and below (s16) +3 + 1; when the box leaves +3 = 3;
// Area65_DrawHud(0x5C, y) - the y pushed with the object pointer's high half
// above the word, which the drawing never reads.
extern "C" void __cdecl Area65_BoxSlideIn(void) {
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
    AH_AT(DrawAt, kDrawHud65)(0x5C, S16(o + 0x30));
}

// original 0x40BF50 (Area65_BoxStates 2; area 45's 0x4081D0): while +0xB is 0
// +9 + 1, at 0x5A and above (u8) +0xB + 1; when the box leaves +3 + 1;
// Area65_DrawHud(0x5C, y).
extern "C" void __cdecl Area65_BoxHold(void) {
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
    AH_AT(DrawAt, kDrawHud65)(0x5C, S16(o + 0x30));
}

// original 0x40BFC0 (Area65_BoxStates 3; area 45's 0x408240): y +0x30 += 10;
// at 0xF0 and above (s16) +3 = 0; unless the mode byte is set (whatever
// +0xB), Field_Request is 2 or Field_ScriptFlags bit 8, +3 = 1;
// Area65_DrawHud(0x5C, y).
extern "C" void __cdecl Area65_BoxSlideOut(void) {
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
    AH_AT(DrawAt, kDrawHud65)(0x5C, S16(o + 0x30));
}

// original 0x40C020 (area 45's Area45_DrawFrame 0x4082A0, instruction for
// instruction; its tables 0x604914 / 0x6048BC): the dial frame at (x, y),
// nothing unless Draw_PassFlags & 0x1B. A draw-mode primitive
// (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)) committed to slot 1; the dial
// (sprite 0) at (x, y); the cell under the leader (AreaMap_ByteAt of the high
// words of Field_Kind2X / Z); legend 1 at (x + 0x30, y) unless the cell is
// 0xA0 / 0xA1 / 0xAE or Field_ScriptFlags2 bit 12, its key (the first of six
// button entries whose mask has a bit of the button word 0) at (x + 0x38, y +
// 8) as the entry's sprite + 1 (lit) or the sprite (withheld); legend 2 at (x
// + 0x30, y + 0x10) unless the cell is 0xA0 / 0xA1, its key from word 6 over
// EIGHT entries (the seventh and eighth are Area65_Record8States' words) at
// (x + 0x38, y + 0x10); legend 3 at (x + 0x30, y + 0x18) when
// Field_CellHasEvent(cell x, z) or flag bit 12, or the party set is 0xC, or
// Field_ScriptFlags bit 14; the needle at (x + 0x18, y + 0x18).
extern "C" void __cdecl Area65_DrawFrame(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    AH_AT(DrawSpriteAt, kDrawSprite65)(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                                       static_cast<short>(Word(At(at::kLeaderCellZ))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) AH_AT(DrawSpriteAt, kDrawSprite65)(x + 0x30, y, 1);
    {
        const int key = KeyIndex(static_cast<U>(Long(At(at::kButtonMap0))), 6);
        if (key >= 0) AH_AT(DrawSpriteAt, kDrawSprite65)(x + 0x38, y + 8, static_cast<unsigned>(withheld1 ? key : ((key + 1) & 0xFF)));
    }
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) AH_AT(DrawSpriteAt, kDrawSprite65)(x + 0x30, y + 0x10, 2);
    {
        const int key = KeyIndex(static_cast<U>(Long(At(at::kButtonMap6))), 8);
        if (key >= 0) AH_AT(DrawSpriteAt, kDrawSprite65)(x + 0x38, y + 0x10, static_cast<unsigned>(withheld2 ? key : ((key + 1) & 0xFF)));
    }
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                            static_cast<short>(Word(At(at::kLeaderCellZ)))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (At(at::kPartySet)[0] & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) AH_AT(DrawSpriteAt, kDrawSprite65)(x + 0x30, y + 0x18, 3);
    AH_CALL(WorldMap_DrawNeedle)(x + 0x18, y + 0x18);
}

// original 0x40C1F0 (area 45's Area45_DrawSprite 0x408470; its table
// 0x6048BC): one sprite of the dial page at (x, y): a draw-mode primitive
// committed to slot 1, then at Gfx_PacketNext (read again) a SPRT,
// semi-transparent when the index's low byte is not 0, colour 0x80 x 3, x
// and y as floats of the arguments' low words, CLUT 0x7B80, (w, h, u, v) from
// Area65_Sprites by index & 0xFF (unchecked); Gfx_CommitPrim(1, 0x1C).
extern "C" void __cdecl Area65_DrawSprite(int x, int y, unsigned index) {
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetSprt)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, (index & 0xFF) != 0 ? 1u : 0u);
    const float fx = static_cast<float>(static_cast<short>(x));
    const float fy = static_cast<float>(static_cast<short>(y));
    std::memcpy(prim + 8, &fx, 4);
    prim[4] = prim[5] = prim[6] = 0x80;
    std::memcpy(prim + 0xC, &fy, 4);
    SetWord(prim + 0x16, 0x7B80);
    const unsigned char* const entry = At(at::kA65Sprites + (index & 0xFF) * 4u);
    SetWord(prim + 0x18, entry[0]);
    SetWord(prim + 0x1A, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    AH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x40C2B0 (area 45's Area45_DrawHud 0x4086D0): nothing unless
// Draw_PassFlags & 0x1B; the region box (sprite 4) at (x, y), its cap (5) at
// (x + 0x80, y), then Text_DrawAt(x + 4, y + 4, 0, 0xFF, 0x803580 + the low
// word of the dword 0x803584, read after the sprites - area 45's cell; area
// 16's copy reads 0x803588).
extern "C" void __cdecl Area65_DrawHud(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, kDrawSprite65)(x, y, 4);
    AH_AT(DrawSpriteAt, kDrawSprite65)(x + 0x80, y, 5);
    const unsigned char* const text = At(at::kAreaText + (static_cast<U>(Long(At(at::kAreaTextOffset))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

// ===========================================================================
// Area 65: the record's slots +8 and +4 (effect kinds 0x16 and 0xE)
// ===========================================================================

// original 0x40C310 (record 3 +8; area 45's 0x408730): Area65_Record8States
// 0x60492C by +1 - 0x4253C0 (another group's, shared by the ten world maps),
// Area65_Record8Place, Area65_Record8Move (shared, this block's).
extern "C" void __cdecl Area65_Record8Run(void) { StateEntry("Area65_Record8Run", at::kA65Record8States, 3, Cur()[1])(); }

// original 0x40C330 (Area65_Record8States 1; area 45's 0x408750):
// Sprite_SetAnimationBank(0x46); +0x48 = +0x24 = 0, +0x29 = 5; the direction
// d = +8 (unchecked): +0xC / +0x10 = the (s16) words of Area65_Directions[d];
// +0x34 / +0x38 = the leader's position. Unless +6 is 0: the words +0x36 /
// +0x3A less each direction word >> 13 (arithmetic, 16-bit), then +0x36 (when
// +0xC is 0) or +0x3A moved by 2 - up for +6 == 1, down otherwise. Then the
// words +0x36 / +0x3A less the direction words >> 9; +0xB = 0, +1 + 1;
// Sprite_SetAnimation(Area65_Record8Anims[d] byte 0), +0x2A = its byte 1 (d
// read again after the call); a tail jump to Sprite_UpdateScreen.
extern "C" void __cdecl Area65_Record8Place(void) {
    const auto dir = [](const unsigned char* o, U half) { return static_cast<std::int16_t>(Word(At(at::kA65Directions + half + o[8] * 4u))); };
    AH_CALL(Sprite_SetAnimationBank)(0x46);
    Cur()[0x48] = 0;
    Cur()[0x24] = 0;
    Cur()[0x29] = 5;
    SetLong(Cur() + 0xC, dir(Cur(), 0));
    SetLong(Cur() + 0x10, dir(Cur(), 2));
    SetLong(Cur() + 0x34, Long(At(at::kLeaderX)));
    SetLong(Cur() + 0x38, Long(At(at::kLeaderZ)));
    unsigned char* o = Cur();
    if (o[6] != 0) {
        SetWord(o + 0x36, Word(o + 0x36) - static_cast<unsigned>(dir(o, 0) >> 13));
        o = Cur();
        SetWord(o + 0x3A, Word(o + 0x3A) - static_cast<unsigned>(dir(o, 2) >> 13));
        o = Cur();
        const bool along_x = Long(o + 0xC) == 0;
        const unsigned step = o[6] == 1 ? 2u : 0xFFFEu;
        if (along_x) SetWord(o + 0x36, Word(o + 0x36) + step);
        else SetWord(o + 0x3A, Word(o + 0x3A) + step);
        o = Cur();
    }
    SetWord(o + 0x36, Word(o + 0x36) - static_cast<unsigned>(dir(o, 0) >> 9));
    o = Cur();
    SetWord(o + 0x3A, Word(o + 0x3A) - static_cast<unsigned>(dir(o, 2) >> 9));
    Cur()[0xB] = 0;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    AH_CALL(Sprite_SetAnimation)(At(at::kA65Record8Anims + Cur()[8] * 2u)[0]);
    o = Cur();
    o[0x2A] = At(at::kA65Record8Anims + 1 + o[8] * 2u)[0];
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x40C490 (the record +8 effect's state 2 of ten world maps - the
// Record8States tables of areas 16, 33, 45, 65, 87, 88, 115, 121, 151, 152
// name it; one body, in area 65's block, a gap between Area65_Record8Place
// and Area65_Record4Run the TSV missed): with +0xB 0, +0 bit 7 clear sets +0xB
// = 1 and the effect moves on; with +0xB set, +0 bit 7 set releases it (a
// tail jump to Effect_Release), clear moves it on. Moving on: +0x34 += +0xC,
// +0x38 += +0x10, Sprite_ScriptTick (its answer not read), then a tail jump
// to Sprite_UpdateScreen. +0 and +0xB are read once, before the store.
extern "C" void __cdecl Area65_Record8Move(void) {
    unsigned char* o = Cur();
    const unsigned char held = o[0xB];
    const unsigned char flags = o[0];
    if (held == 0) {
        if ((flags & 0x80) == 0) {
            o[0xB] = 1;
            o = Cur();
        }
    } else if ((flags & 0x80) != 0) {
        AH_CALL(Effect_Release)();
        return;
    }
    AddLong(o + 0x34, static_cast<U>(Long(o + 0xC)));
    o = Cur();
    AddLong(o + 0x38, static_cast<U>(Long(o + 0x10)));
    AH_CALL(Sprite_ScriptTick)();
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x40C4E0 (record 3 +4; area 45's 0x4088B0): Area65_Record4States
// 0x604950 by +1 - Area65_Record4MarkCell, then Area45_Record4Tick 0x408990
// (shared by ten world maps, AR1B's).
extern "C" void __cdecl Area65_Record4Run(void) { StateEntry("Area65_Record4Run", at::kA65Record4States, 2, Cur()[1])(); }

// original 0x40C500 (Area65_Record4States 0; area 45's 0x4088D0): with the
// byte 0x903A79 9 or Field_StatusBits bit 0, a tail jump to Effect_Release.
// Else WorldMap_RecordIndex (its answer not read),
// Sprite_SetAnimationBank(0x205); +0x48, +0x24, +0x2A, +0x5D, +0x5E, +0x5F =
// 0; the map byte of the cell record Area65_Cells[+0xB] (x, z; unchecked) set
// to 0xA0 - AreaMap_Bytes + AreaMap_Header[0] * z + x; Sprite_SetAnimation(0)
// and +1 = 1.
extern "C" void __cdecl Area65_Record4MarkCell(void) {
    if (At(at::kFlag3A79)[0] == 9 || (Field_StatusBits & 1) != 0) {
        AH_CALL(Effect_Release)();
        return;
    }
    AH_CALL(WorldMap_RecordIndex)();
    AH_CALL(Sprite_SetAnimationBank)(0x205);
    Cur()[0x48] = 0;
    Cur()[0x24] = 0;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    const U width = AreaMap_Header[0];
    const U record = at::kA65Cells + Cur()[0xB] * 4u;
    const U z = At(record + 1)[0];
    const U x = At(record)[0];
    const_cast<unsigned char*>(AreaMap_Bytes)[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = 1;
}

// ===========================================================================
// Area 65: the drift layer (record 3 +0x10)
// ===========================================================================

// original 0x40C5C0 (record 3 +0x10, through WorldMap_RecordHook10; area 45's
// Area45_DrawDrift 0x4089A0 instruction for instruction, its table
// Area65_DriftUV 0x604958): docs/worldmap_area.md section 4. Once (+2 == 0):
// the word +0x3A += Frame_Counter & 0xF, +2 + 1. Nothing more unless
// Draw_PassFlags bit 2. With b = +0xB and e = b - 2: +0x38 += b << 10; +0x3A
// = -8 above the map's height + 8; nothing more unless the leader is within
// 25 cells in x OR in z; one textured square through the GTE, then a grid of
// (17 - 4b) x (16 - 4b) semi-transparent map-item quads with u / v from the
// table by e (unchecked).
extern "C" void __cdecl Area65_DrawDrift(void) {
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
    AH_CALL(Gpu_SetPolyFT4)(prim);
    AH_CALL(Gpu_SetShadeTex)(prim, 0);
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
    using Pers4 = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                                  long*, long*);
    AH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(sv, sv + 4, sv + 8, sv + 12, reinterpret_cast<float*>(prim + 8),
                                                reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                                                reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
    AH_CALL(Gte_PrimDepths4_10)(prim);
    AH_CALL(Prim_SetTexture)(static_cast<U>(e) | 0xBB509100u, prim, 1);
    AH_CALL(Gfx_CommitPrim)(4, 0x48);

    const std::int32_t rows = 9 - 4 * e;    // 17 - 4b
    const std::int32_t columns = 4 * side;  // 16 - 4b
    for (std::int32_t j = 0; j < rows; ++j) {
        for (std::int32_t i = 0; i < columns; ++i) {
            o = Cur();
            unsigned char* const item = AH_CALL(MapView_ItemHalfAt)(S16(o + 0x36) + i + e - 2, S16(o + 0x3A) + j + e - 2);
            if (item == nullptr) continue;
            unsigned char* const q = Gfx_PacketNext;
            AH_CALL(Gpu_SetPolyFT4)(q);
            AH_CALL(Gpu_SetShadeTex)(q, 0);
            AH_CALL(Gpu_SetSemiTrans)(q, 1);
            std::memcpy(q + 0x08, item + 0x08, 12);
            std::memcpy(q + 0x18, item + 0x18, 12);
            std::memcpy(q + 0x28, item + 0x28, 12);
            std::memcpy(q + 0x38, item + 0x38, 12);
            SetWord(q + 0x16, 0x78CB);
            SetWord(q + 0x26, 0x5B);
            q[4] = q[5] = q[6] = 0x28;
            const U m = At(at::kA65DriftSize + static_cast<U>(e))[0];
            const U ubase = At(at::kA65DriftUBase + static_cast<U>(e))[0];
            const U vbase = At(at::kA65DriftVBase + static_cast<U>(e))[0];
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
            AH_CALL(MapView_LinkPrimAt)(x, z, 1, 0x48);
        }
    }
}

// ===========================================================================
// Area 67: three choices
// ===========================================================================

namespace {
// The message a choice handler opens: table[(s8) choice], unchecked.
signed char Choice() { return static_cast<signed char>(B(at::kChoice)); }
void ChoiceMessage(U table, signed char choice) { SetWord(At(at::kMessage), Word(At(table + static_cast<U>(choice * 2)))); }
}  // namespace

// original 0x40CA30 (area 67's choice 0, the descriptor 0x607000's +0x34 =
// 0x606FF4; its PSX descriptor 0x801F5340): message = Area67_MessagesA
// 0x607044 by the s8 choice (unchecked); choice 5 also sets the byte 0x903848
// to 6.
extern "C" void __cdecl Area67_ChoiceMark6(void) {
    const signed char choice = Choice();
    ChoiceMessage(at::kA67MessagesA, choice);
    if (choice == 5) B(at::kScratch48) = 6;
}

// original 0x40CA60 (area 67's choice 1): message = Area67_MessagesB 0x607050
// by the s8 choice (unchecked).
extern "C" void __cdecl Area67_ChoiceMessage(void) { ChoiceMessage(at::kA67MessagesB, Choice()); }

// original 0x40CA80 (area 67's choice 2): message = Area67_MessagesC 0x60705C
// by the s8 choice (unchecked); choices 0 and 1 (signed) also set the byte
// 0x903848 to 5.
extern "C" void __cdecl Area67_ChoiceMark5(void) {
    const signed char choice = Choice();
    ChoiceMessage(at::kA67MessagesC, choice);
    if (choice >= 0 && choice <= 1) B(at::kScratch48) = 5;
}

// ===========================================================================
// Area 67: nineteen handlers (Area67_Handlers 0x606F20; PSX 0x801F3EC4..)
// ===========================================================================

// original 0x40CAB0 (area 67's handler 0, also area 108's; PSX 0x801F3EC4):
// Sprite_Current +0x24 |= 0x10.
extern "C" void __cdecl Area67_SetBit24(void) { Cur()[0x24] = static_cast<unsigned char>(Cur()[0x24] | 0x10); }

// original 0x40CAC0 (handler 1, also area 131's; PSX 0x801F3EE4): field object
// 0's +0x34 (Sprite_Objects 0x7DEEB4, 16.16) += 0x800.
extern "C" void __cdecl Area67_Object0XUp(void) { AddLong(At(at::kObject0X), 0x800); }

// original 0x40CAD0 (handler 2; PSX 0x801F3F00): Effect_FindFree; a slot: its
// record +0 = 1, kind +5 = 0x2D, +0x34 / +0x38 = the running object's
// (Sprite_Current read for each).
extern "C" void __cdecl Area67_SpawnEffect2D(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x2D;
    SetLong(e + 0x34, Long(Cur() + 0x34));
    SetLong(e + 0x38, Long(Cur() + 0x38));
}

namespace {
// Handlers 3..5: party member k's record (ObjTrio + k * 0x14C) becomes
// Sprite_Current, its +0x34 / +0x38 / +0x3C (read first) the position of
// Effect_SpawnAt(3, 0, Area67_SpawnAtArg[the member's character id]); an
// answer other than 0xFF goes to Sprite_Current +0xB (read again).
void MemberSpawnAt(unsigned k) {
    unsigned char* const member = At(at::kLeader + k * at::kPartyStride);
    const std::int32_t x = Long(member + 0x34), y = Long(member + 0x38), z = Long(member + 0x3C);
    const unsigned id = B(at::kPartyList + k);
    Sprite_Current = member;
    const auto arg = static_cast<signed char>(B(at::kA67SpawnAtArg + id));
    const unsigned char r = AH_CALL(Effect_SpawnAt)(3, 0, arg, x, y, z);
    if (r != 0xFF) Cur()[0xB] = r;
}
// Handlers 6..11 and 16: the same with Effect_Spawn(kind, 0, table[id], the
// member's words +0x2E / +0x30).
void MemberSpawn(unsigned k, unsigned char kind, U table) {
    unsigned char* const member = At(at::kLeader + k * at::kPartyStride);
    const auto x = static_cast<short>(Word(member + 0x2E));
    const auto z = static_cast<short>(Word(member + 0x30));
    const unsigned id = B(at::kPartyList + k);
    Sprite_Current = member;
    const auto arg = static_cast<signed char>(B(table + id));
    const unsigned char r = AH_CALL(Effect_Spawn)(kind, 0, arg, x, z);
    if (r != 0xFF) Cur()[0xB] = r;
}
}  // namespace

// original 0x40CB10 / 0x40CB60 / 0x40CBB0 (handlers 3, 4, 5; PSX 0x801F3F80,
// 0x801F400C, 0x801F4098): MemberSpawnAt for members 0, 1, 2 - the character
// id's byte of Area67_SpawnAtArg 0x604BB8 (unchecked).
extern "C" void __cdecl Area67_Member0SpawnAt(void) { MemberSpawnAt(0); }
extern "C" void __cdecl Area67_Member1SpawnAt(void) { MemberSpawnAt(1); }
extern "C" void __cdecl Area67_Member2SpawnAt(void) { MemberSpawnAt(2); }

// original 0x40CC00 / 0x40CC50 / 0x40CCA0 (handlers 6, 7, 8; PSX 0x801F4124,
// 0x801F41A4, 0x801F4224): Effect_Spawn kind 3 at members 0, 1, 2 with the
// same table.
extern "C" void __cdecl Area67_Member0Spawn3(void) { MemberSpawn(0, 3, at::kA67SpawnAtArg); }
extern "C" void __cdecl Area67_Member1Spawn3(void) { MemberSpawn(1, 3, at::kA67SpawnAtArg); }
extern "C" void __cdecl Area67_Member2Spawn3(void) { MemberSpawn(2, 3, at::kA67SpawnAtArg); }

// original 0x40CCF0 / 0x40CD40 / 0x40CD90 (handlers 9, 10, 11; PSX 0x801F42A4,
// 0x801F4324, 0x801F43A4): Effect_Spawn kind 1 at members 0, 1, 2, their byte
// of Area67_SpawnArgB 0x604BC0 (unchecked).
extern "C" void __cdecl Area67_Member0Spawn1(void) { MemberSpawn(0, 1, at::kA67SpawnArgB); }
extern "C" void __cdecl Area67_Member1Spawn1(void) { MemberSpawn(1, 1, at::kA67SpawnArgB); }
extern "C" void __cdecl Area67_Member2Spawn1(void) { MemberSpawn(2, 1, at::kA67SpawnArgB); }

// original 0x40CDE0 (handler 12, also area 131's; PSX 0x801F4424): field
// object 0's +0x34 -= 0x800.
extern "C" void __cdecl Area67_Object0XDown(void) { AddLong(At(at::kObject0X), static_cast<U>(-0x800)); }

// original 0x40CDF0 (handler 13; PSX 0x801F4440): field object 1's +0x34
// (0x7DEF58) += 0x800.
extern "C" void __cdecl Area67_Object1XUp(void) { AddLong(At(at::kObject1X), 0x800); }

// original 0x40CE00 (handler 14; PSX 0x801F445C): field object 1's +0x34 -=
// 0x800.
extern "C" void __cdecl Area67_Object1XDown(void) { AddLong(At(at::kObject1X), static_cast<U>(-0x800)); }

// original 0x40CE10 (handler 15, also handlers of areas 7, 19 and 130 - one
// body, in area 67's block; PSX 0x801F4478): Music_FadeOutStop(10).
extern "C" void __cdecl Area67_MusicFade10(void) { AH_CALL(Music_FadeOutStop)(10); }

// original 0x40CE20 (handler 16; PSX 0x801F449C): Effect_Spawn kind 4 at the
// leader, the leader's byte of Area67_SpawnArgC 0x604BC8 (unchecked).
extern "C" void __cdecl Area67_LeaderSpawn4(void) { MemberSpawn(0, 4, at::kA67SpawnArgC); }

// original 0x40CE70 (handler 17; PSX 0x801F451C): Field_ActiveMember +0x80
// bit 0 cleared (the byte read before the test); when the leader's +0x89
// (MoveScript_EffectState's index) is 6, then ScriptFlags_Set40,
// MoveScript_Var7 = 4, the four bytes 0x903848..0x90384B and 0x8034E5 zeroed.
extern "C" void __cdecl Area67_ResetOn6(void) {
    const unsigned char effect = B(at::kLeaderEffect);
    unsigned char* const member = Field_ActiveMember;
    const auto cleared = static_cast<unsigned char>(member[0x80] & 0xFE);
    member[0x80] = cleared;
    if (effect != 6) return;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kVar7) = 4;
    B(at::kScratch48) = 0;
    B(at::kScratch48 + 1) = 0;
    B(at::kScratch48 + 2) = 0;
    B(at::kScratch48 + 3) = 0;
    B(at::kVar8) = 0;
}

// original 0x40CEC0 (handler 18; PSX 0x801F45BC): AreaMap_SetByte(0x25, 0xB,
// 0) - the map cell (0x25, 0xB) cleared.
extern "C" void __cdecl Area67_ClearCell(void) { AH_CALL(AreaMap_SetByte)(0x25, 0xB, 0); }

// original 0x40CED0 (Field_ObjectTriggers id 31, 0x662E98, called (object,
// 0x904030) and reading neither; a gap function by the tool, placed in area
// 67's block by address): ScriptFlags_Set40, then the mode tail armed - kind
// 0x2C (the engine's 0x56DE50), state 0, argument 0x9039F5 = 1. al 0.
extern "C" unsigned char __cdecl Area67_Trigger31(void) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x2C;
    B(at::kTailState) = 0;
    B(at::kTailArg) = 1;
    return 0;
}

// ===========================================================================

void AreaW1e_Inject() {
    if (bof3::WantsShadow("area_w1e")) area_w1e::SelfTest();
    BOF3_INJECT(Area65_PlaceMessage);
    BOF3_INJECT(Area65_PlateRun);
    BOF3_INJECT(Area65_PlateStart);
    BOF3_INJECT(Area65_PlateShow);
    BOF3_INJECT(Area65_PlateGrow);
    BOF3_INJECT(Area65_PlateHold);
    BOF3_INJECT(Area65_PlateShrink);
    BOF3_INJECT(Area65_HudRun);
    BOF3_INJECT(Area65_HudFrame);
    BOF3_INJECT(Area65_FrameStep);
    BOF3_INJECT(Area65_FrameSlideIn);
    BOF3_INJECT(Area65_FrameHold);
    BOF3_INJECT(Area65_FrameSlideOut);
    BOF3_INJECT(Area65_BoxStep);
    BOF3_INJECT(Area65_BoxSlideIn);
    BOF3_INJECT(Area65_BoxHold);
    BOF3_INJECT(Area65_BoxSlideOut);
    BOF3_INJECT(Area65_DrawFrame);
    BOF3_INJECT(Area65_DrawSprite);
    BOF3_INJECT(Area65_DrawHud);
    BOF3_INJECT(Area65_Record8Run);
    BOF3_INJECT(Area65_Record8Place);
    BOF3_INJECT(Area65_Record8Move);
    BOF3_INJECT(Area65_Record4Run);
    BOF3_INJECT(Area65_Record4MarkCell);
    BOF3_INJECT(Area65_DrawDrift);
    BOF3_INJECT(Area67_ChoiceMark6);
    BOF3_INJECT(Area67_ChoiceMessage);
    BOF3_INJECT(Area67_ChoiceMark5);
    BOF3_INJECT(Area67_SetBit24);
    BOF3_INJECT(Area67_Object0XUp);
    BOF3_INJECT(Area67_SpawnEffect2D);
    BOF3_INJECT(Area67_Member0SpawnAt);
    BOF3_INJECT(Area67_Member1SpawnAt);
    BOF3_INJECT(Area67_Member2SpawnAt);
    BOF3_INJECT(Area67_Member0Spawn3);
    BOF3_INJECT(Area67_Member1Spawn3);
    BOF3_INJECT(Area67_Member2Spawn3);
    BOF3_INJECT(Area67_Member0Spawn1);
    BOF3_INJECT(Area67_Member1Spawn1);
    BOF3_INJECT(Area67_Member2Spawn1);
    BOF3_INJECT(Area67_Object0XDown);
    BOF3_INJECT(Area67_Object1XUp);
    BOF3_INJECT(Area67_Object1XDown);
    BOF3_INJECT(Area67_MusicFade10);
    BOF3_INJECT(Area67_LeaderSpawn4);
    BOF3_INJECT(Area67_ResetOn6);
    BOF3_INJECT(Area67_ClearCell);
    BOF3_INJECT(Area67_Trigger31);
}
