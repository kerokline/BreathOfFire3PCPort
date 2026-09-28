// World 0's areas 16 and 18..26: the code of the PSX's BIN/WORLD00/AREA016.EMI
// and AREA018..026.EMI compiled into the exe at 0x401B80..0x4033F2 - 61
// functions, each read to its last instruction with capstone (2026-09-27) and
// taken through the area harness (area_harness.h). Round ten group AR0B;
// docs/area_w0b.md has the areas one section each.
//
// Area 16 is a world map (WorldMap_Records' record 0): its code is a copy of
// area 33's (docs/worldmap_area.md, docs/world-map.md: every instruction the
// same but the tables' addresses), reached through the record's slots, the
// field-hook table and its own state tables, plus a place-message hook of its
// own and the two record slots +4 / +8 whose area-33 copies are not ours yet.
// Areas 18..26 are small: handlers, choice handlers and two inits.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Every
// call goes through the harness (AH_CALL / AH_AT), and every dispatch through
// a .data table reads the table in place, the index unchecked as the
// original's, so the start-up fuzz can stand recorders in for the entries.
#include "game/area_w0b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w0b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0b::at;
using U = std::uint32_t;
using area_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char* Pointer(U cell) { return At(static_cast<U>(Long(At(cell)))); }
// A handler read from a .data table in place, as the originals' `call` /
// `jmp [index * 4 + table]`: the index is not checked.
Handler Entry(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(address))))); }
void SetMessage(unsigned id) { SetWord(At(at::kMessage), id); }
signed char Choice() { return static_cast<signed char>(At(at::kChoice)[0]); }

// Area 16's own functions another of them calls directly, by their original
// addresses (each is patched to ours; the fuzz stands a recorder in).
constexpr U kFrameStep16 = bof3::addr::Area16_FrameStep, kFrameHold16 = bof3::addr::Area16_FrameHold,
    kBoxStep16 = bof3::addr::Area16_BoxStep;
constexpr U kDrawFrame16 = bof3::addr::Area16_DrawFrame, kDrawSprite16 = bof3::addr::Area16_DrawSprite,
    kDrawHud16 = bof3::addr::Area16_DrawHud;
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
        const unsigned char* const entry = At(at::kA16Buttons + k * 4);
        if ((Word(entry) & button_word & 0xFFFF) != 0) return entry[2];
    }
    return -1;
}

}  // namespace

// ===========================================================================
// Area 16: the world map's field hook (WorldMap_FieldHooks entry 0)
// ===========================================================================

// original 0x401B80 (WorldMap_FieldHooks 0x662DF0 entry 0; 0x56DE30 calls the
// entry of WorldMap_RecordIndex): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the cell the leader stands on (the high words of its
//      +0x34 / +0x38) asked of AreaMap_ByteAt. 0xA1 (a place): the message of
//      row (the first of the nine 0x20-byte rows at 0x5E5E2C whose word is the
//      place 0x937F82, 9 when none) * 16 + (s8) Cond_ByteFA, Msg_OpenScript.
//      Any other: the cell's 4-byte record of 0x5E5C04 whose (x, z) bytes are
//      the leader's cell words (read again after the call; searched with NO
//      bound), its id byte +3; the name set of 0x5E5F4C whose id it is (3 when
//      none); up to four Text_Records rows from its item bytes (0xFF ends
//      them): "????????" and a 0 for an item whose byte at 0x9040EC is 0, else
//      the item's 16-byte name (0x669CD8 for item 0x16, else Item_NamePtr(0,
//      item + 0x38)); Msg_OpenScript(set + 0x16). Then 0x9039F4 (read again)
//      + 1 and Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original: neither the row, the byte
// nor the set 3 (which reads the plate state table's bytes) is checked.
extern "C" void __cdecl Area16_PlaceMessage(void) {
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
        for (U a = at::kA16PlaceMessages; a < at::kA16PlaceMessagesEnd; a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(at::kA16PlaceMessages + static_cast<U>(index * 2)))));
    } else {
        const unsigned cx = Word(At(at::kLeaderCellWordX));
        const unsigned cz = Word(At(at::kLeaderCellWordZ));
        U cell = at::kA16Cells;
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = at::kA16NameSets; a < at::kA16NameSetsEnd; a += 5, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(at::kA16NameSets + 1 + set * 5);
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
// Area 16: the place plate (world-map record 0 +0: effect kind 0)
// ===========================================================================

// original 0x401D00 (WorldMap_Records[0] +0; area 33's copy 0x403E00): the
// cell ahead of the leader - the high words of (leader +9) * (+0xC) + (+0x34)
// and of the same with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the
// kind +0xB by AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2,
// 0xAE 3, else Field_ScriptFlags2 bit 12 4, else 0; then Area16_PlateStates
// 0x5E5F5C by +1 (read after the calls, unchecked).
extern "C" void __cdecl Area16_PlateRun(void) {
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
    Entry(at::kA16PlateStates + Cur()[1] * 4u)();
}

// original 0x401DE0 (Area16_PlateStates 0 and WorldMap33_PlateStates 0: one
// body for both maps): +0x24 = 0x80, Sprite_SetAnimationBank(0xF), then +0x29
// = 5, +0x2A, +0x5D, +0x5E, +0x5F = 0 and +1 = 1 (the show), Sprite_Current
// read again for each store.
extern "C" void __cdecl Area16_PlateStart(void) {
    Cur()[0x24] = 0x80;
    AH_CALL(Sprite_SetAnimationBank)(0xF);
    Cur()[0x29] = 5;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    Cur()[1] = 1;
}

// original 0x401E30 (Area16_PlateStates 1; area 33's 0x403EE0): +7 = +0xB.
// Kind 1: the entry of Area16_PlateAnims 0x5E5BE0 whose word is the place
// 0x937F82 - searched with NO bound - +0x18 = the place (zero-extended), its
// animation byte. Kinds 2, 3, 4: animations 3, 0, 1. Each: +0x40 = 0, +0x44 =
// 0x10000, +0x48 = 2, +9 = 8, Sprite_SetAnimation, +1 = 2. Other kinds: no more.
extern "C" void __cdecl Area16_PlateShow(void) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = Word(At(at::kPlace));
        U entry = at::kA16PlateAnims;
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

// original 0x401F80 (Area16_PlateStates 2; area 33's 0x404030):
// WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0 +0x48 = 0 and +1 = 3;
// then a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area16_PlateGrow(void) {
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

// original 0x401FD0 (Area16_PlateStates 3; area 33's 0x404080):
// WorldMap_PinSprite; unless Game_Mode is 1 the plate leaves (+0x48 = 2, +9 =
// 8, +1 = 4) when +0xB is not +7, or Field_Request is 5, or +7 is 1 and +0x18
// is not the place (zero-extended); a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl Area16_PlateHold(void) {
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

// original 0x402030 (Area16_PlateStates 4; area 33's 0x4040E0):
// WorldMap_PinSprite; +0x40 -= 0x2000; +9 - 1. Not 0: a tail jump to
// Sprite_QueueOverlay. At 0: Field_Request 5 a tail jump to Effect_Release,
// else +0x48 = 0 and +1 = 1.
extern "C" void __cdecl Area16_PlateShrink(void) {
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
// Area 16: the HUD task (record 0 +0xC: effect kind 0x58), the dial frame's
// slide and the region box
// ===========================================================================

// original 0x402080 (record 0 +0xC; area 33's 0x404130): Area16_HudStates
// 0x5E5F70 by +1 - WorldMapHud_Start (shared), Area16_HudFrame.
extern "C" void __cdecl Area16_HudRun(void) { Entry(at::kA16HudStates + Cur()[1] * 4u)(); }

// original 0x4020A0 (Area16_HudStates 1; area 33's 0x404150): `call
// 0x4020B0; jmp 0x402180` - the frame's slide, then the region box's.
extern "C" void __cdecl Area16_HudFrame(void) {
    area_harness::Phase(kFrameStep16)();
    area_harness::Phase(kBoxStep16)();
}

// original 0x4020B0 (area 33's WorldMap_FrameStep's dispatch): Area16_FrameStates
// 0x5E5F78 by +2 - WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold,
// _FrameSlideOut. A tail jump; the index unchecked.
extern "C" void __cdecl Area16_FrameStep(void) { Entry(at::kA16FrameStates + Cur()[2] * 4u)(); }

// original 0x4020D0 (Area16_FrameStates 1): the word +0x2E += 0x10; at 0x10
// and above (s16) +2 + 1; then a tail jump to Area16_FrameHold.
extern "C" void __cdecl Area16_FrameSlideIn(void) {
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) + 0x10u);
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(kFrameHold16)();
}

// original 0x402100 (Area16_FrameStates 2): the mode byte 2 makes +2 = 3;
// Area16_DrawFrame(0x10, the word +0x2E). The y pushed carries a stale high
// half the drawing never reads: ours passes the word sign-extended.
extern "C" void __cdecl Area16_FrameHold(void) {
    if (At(at::kMapMode)[0] == 2) Cur()[2] = 3;
    AH_AT(DrawAt, kDrawFrame16)(0x10, S16(Cur() + 0x2E));
}

// original 0x402130 (Area16_FrameStates 3): the word +0x2E -= 0x10; at -0x30
// and below +2 = 0; unless the mode byte is 2, +2 = 1 (over the 0);
// Area16_DrawFrame(0x10, y).
extern "C" void __cdecl Area16_FrameSlideOut(void) {
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
    AH_AT(DrawAt, kDrawFrame16)(0x10, S16(o + 0x2E));
}

// original 0x402180 (area 33's WorldMapHud_BoxStep 0x404230): Area16_BoxStates
// 0x5E5F88 by +3 - WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold,
// _BoxSlideOut.
extern "C" void __cdecl Area16_BoxStep(void) { Entry(at::kA16BoxStates + Cur()[3] * 4u)(); }

// original 0x4021A0 (Area16_BoxStates 1; area 33's 0x404250): y +0x30 -= 10;
// at 0xC8 and below (s16) +3 + 1; when the box leaves +3 = 3;
// Area16_DrawHud(0x5C, y) - the y pushed with the object pointer's high half
// above the word, which the drawing never reads.
extern "C" void __cdecl Area16_BoxSlideIn(void) {
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
    AH_AT(DrawAt, kDrawHud16)(0x5C, S16(o + 0x30));
}

// original 0x402210 (Area16_BoxStates 2; area 33's 0x4042C0): while +0xB is 0
// +9 + 1, at 0x5A and above (u8) +0xB + 1; when the box leaves +3 + 1;
// Area16_DrawHud(0x5C, y).
extern "C" void __cdecl Area16_BoxHold(void) {
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
    AH_AT(DrawAt, kDrawHud16)(0x5C, S16(o + 0x30));
}

// original 0x402280 (Area16_BoxStates 3; area 33's 0x404330): y +0x30 += 10;
// at 0xF0 and above (s16) +3 = 0; unless the mode byte is set (whatever
// +0xB), Field_Request is 2 or Field_ScriptFlags bit 8, +3 = 1;
// Area16_DrawHud(0x5C, y).
extern "C" void __cdecl Area16_BoxSlideOut(void) {
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
    AH_AT(DrawAt, kDrawHud16)(0x5C, S16(o + 0x30));
}

// original 0x4022E0 (area 33's WorldMap_DrawFrame 0x404390, instruction for
// instruction; its tables 0x5E5FF0 / 0x5E5F98): the dial frame at (x, y),
// nothing unless Draw_PassFlags & 0x1B. A draw-mode primitive
// (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)) committed to slot 1; the dial
// (sprite 0) at (x, y); the cell under the leader (AreaMap_ByteAt of the high
// words of Field_Kind2X / Z); legend 1 at (x + 0x30, y) unless the cell is
// 0xA0 / 0xA1 / 0xAE or Field_ScriptFlags2 bit 12, its key (the first of six
// button entries whose mask has a bit of the button word 0) at (x + 0x38, y +
// 8) as the entry's sprite + 1 (lit) or the sprite (withheld); legend 2 at (x
// + 0x30, y + 0x10) unless the cell is 0xA0 / 0xA1, its key from word 6 over
// EIGHT entries (the seventh and eighth are Area16_Record8States' words) at
// (x + 0x38, y + 0x10); legend 3 at (x + 0x30, y + 0x18) when
// Field_CellHasEvent(cell x, z) or flag bit 12, or the party set is 0xC, or
// Field_ScriptFlags bit 14; the needle at (x + 0x18, y + 0x18).
extern "C" void __cdecl Area16_DrawFrame(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    AH_AT(DrawSpriteAt, kDrawSprite16)(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                                       static_cast<short>(Word(At(at::kLeaderCellZ))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) AH_AT(DrawSpriteAt, kDrawSprite16)(x + 0x30, y, 1);
    {
        const int key = KeyIndex(static_cast<U>(Long(At(at::kButtonMap0))), 6);
        if (key >= 0) AH_AT(DrawSpriteAt, kDrawSprite16)(x + 0x38, y + 8, static_cast<unsigned>(withheld1 ? key : ((key + 1) & 0xFF)));
    }
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) AH_AT(DrawSpriteAt, kDrawSprite16)(x + 0x30, y + 0x10, 2);
    {
        const int key = KeyIndex(static_cast<U>(Long(At(at::kButtonMap6))), 8);
        if (key >= 0) AH_AT(DrawSpriteAt, kDrawSprite16)(x + 0x38, y + 0x10, static_cast<unsigned>(withheld2 ? key : ((key + 1) & 0xFF)));
    }
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                            static_cast<short>(Word(At(at::kLeaderCellZ)))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (At(at::kPartySet)[0] & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) AH_AT(DrawSpriteAt, kDrawSprite16)(x + 0x30, y + 0x18, 3);
    AH_CALL(WorldMap_DrawNeedle)(x + 0x18, y + 0x18);
}

// original 0x4024B0 (area 33's WorldMap_DrawSprite 0x404560; its table
// 0x5E5F98): one sprite of the dial page at (x, y): a draw-mode primitive
// committed to slot 1, then at Gfx_PacketNext (read again) a SPRT,
// semi-transparent when the index's low byte is not 0, colour 0x80 x 3, x
// and y as floats of the arguments' low words, CLUT 0x7B80, (w, h, u, v) from
// Area16_Sprites by index & 0xFF (unchecked); Gfx_CommitPrim(1, 0x1C).
extern "C" void __cdecl Area16_DrawSprite(int x, int y, unsigned index) {
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
    const unsigned char* const entry = At(at::kA16Sprites + (index & 0xFF) * 4u);
    SetWord(prim + 0x18, entry[0]);
    SetWord(prim + 0x1A, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    AH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x402570 (area 33's WorldMap_DrawHud 0x404620): nothing unless
// Draw_PassFlags & 0x1B; the region box (sprite 4) at (x, y), its cap (5) at
// (x + 0x80, y), then Text_DrawAt(x + 4, y + 4, 0, 0xFF, 0x803580 + the low
// word of the dword 0x803588, read after the sprites).
extern "C" void __cdecl Area16_DrawHud(int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, kDrawSprite16)(x, y, 4);
    AH_AT(DrawSpriteAt, kDrawSprite16)(x + 0x80, y, 5);
    const unsigned char* const text = At(at::kAreaText + (static_cast<U>(Long(At(at::kAreaTextOffset))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

// ===========================================================================
// Area 16: the record's slots +8 and +4 (effect kinds 0x16 and 0xE)
// ===========================================================================

// original 0x4025D0 (record 0 +8): Area16_Record8States 0x5E6008 by +1 -
// 0x4253C0 and 0x40C490 (the world-map copies' shared states, not this
// group's) around Area16_Record8Place.
extern "C" void __cdecl Area16_Record8Run(void) { Entry(at::kA16Record8States + Cur()[1] * 4u)(); }

// original 0x4025F0 (Area16_Record8States 1; area 33's copy 0x4046A0):
// Sprite_SetAnimationBank(0x46); +0x48 = +0x24 = 0, +0x29 = 5; the direction
// d = +8 (unchecked): +0xC / +0x10 = the (s16) words of Area16_Directions[d];
// +0x34 / +0x38 = the leader's position. Unless +6 is 0: the words +0x36 /
// +0x3A less each direction word >> 13 (arithmetic, 16-bit), then +0x36 (when
// +0xC is 0) or +0x3A moved by 2 - up for +6 == 1, down otherwise. Then the
// words +0x36 / +0x3A less the direction words >> 9; +0xB = 0, +1 + 1;
// Sprite_SetAnimation(Area16_Record8Anims[d] byte 0), +0x2A = its byte 1 (d
// read again after the call); a tail jump to Sprite_UpdateScreen.
extern "C" void __cdecl Area16_Record8Place(void) {
    const auto dir = [](const unsigned char* o, U half) { return static_cast<std::int16_t>(Word(At(at::kA16Directions + half + o[8] * 4u))); };
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
    AH_CALL(Sprite_SetAnimation)(At(at::kA16Record8Anims + Cur()[8] * 2u)[0]);
    o = Cur();
    o[0x2A] = At(at::kA16Record8Anims + 1 + o[8] * 2u)[0];
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x402750 (record 0 +4): Area16_Record4States 0x5E602C by +1 -
// Area16_Record4MarkCell, then 0x408990 (the copies' shared state, not this
// group's).
extern "C" void __cdecl Area16_Record4Run(void) { Entry(at::kA16Record4States + Cur()[1] * 4u)(); }

// original 0x402770 (Area16_Record4States 0; area 33's copy 0x404820): with
// the byte 0x903A79 9 or Field_StatusBits bit 0, a tail jump to
// Effect_Release. Else WorldMap_RecordIndex (its answer not read),
// Sprite_SetAnimationBank(0x205); +0x48, +0x24, +0x2A, +0x5D, +0x5E, +0x5F =
// 0; the map byte of the cell record Area16_Cells[+0xB] (x, z; unchecked) set
// to 0xA0 - AreaMap_Bytes + AreaMap_Header[0] * z + x; Sprite_SetAnimation(0)
// and +1 = 1.
extern "C" void __cdecl Area16_Record4MarkCell(void) {
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
    const U record = at::kA16Cells + Cur()[0xB] * 4u;
    const U z = At(record + 1)[0];
    const U x = At(record)[0];
    const_cast<unsigned char*>(AreaMap_Bytes)[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = 1;
}

// ===========================================================================
// Area 16: the drift layer (record 0 +0x10)
// ===========================================================================

// original 0x402830 (record 0 +0x10, through WorldMap_RecordHook10; area 33's
// WorldMap33_DrawDrift 0x4048E0 instruction for instruction, its table
// Area16_DriftUV 0x5E6034): docs/worldmap_area.md section 4. Once (+2 == 0):
// the word +0x3A += Frame_Counter & 0xF, +2 + 1. Nothing more unless
// Draw_PassFlags bit 2. With b = +0xB and e = b - 2: +0x38 += b << 10; +0x3A
// = -8 above the map's height + 8; nothing more unless the leader is within
// 25 cells in x OR in z; one textured square through the GTE, then a grid of
// (17 - 4b) x (16 - 4b) semi-transparent map-item quads with u / v from the
// table by e (unchecked).
extern "C" void __cdecl Area16_DrawDrift(void) {
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
            const U m = At(at::kA16DriftSize + static_cast<U>(e))[0];
            const U ubase = At(at::kA16DriftUBase + static_cast<U>(e))[0];
            const U vbase = At(at::kA16DriftVBase + static_cast<U>(e))[0];
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
// Area 18
// ===========================================================================

// original 0x402CA0 (area 18's init, the descriptor 0x5E6688's +0x40; PSX
// 0x801F2C04): the byte +4 of the record 0x668D80[Game_AreaNumber] points at
// is 0 when Cond_ByteFA is 8, else 2.
extern "C" void __cdecl Area18_SetRecordByte(void) {
    const unsigned value = Cond_ByteFA == 8 ? 0u : 2u;
    Pointer(at::kAreaRecords + Game_AreaNumber * 4u)[4] = static_cast<unsigned char>(value);
}

// original 0x402CE0 (area 18's handler 0, Area18_Handlers 0x5E6680; PSX
// 0x801F2C68): unless the byte +0x89 of Field_State's record is 2, the word
// +0xA of MoveScript_Object (its script position) + 9.
extern "C" void __cdecl Area18_SkipScript(void) {
    if (Field_State[0x89] == 2) return;
    unsigned char* const object = MoveScript_Object;
    SetWord(object + 0xA, Word(object + 0xA) + 9u);
}

// original 0x402D00 (area 18's handler 1 and area 19's handler 6: one body;
// PSX 0x801F2CA8): eight map cells to 0 through AreaMap_SetByte - x 0x1F and
// 0x20 of rows 0x4C, 0x4D, 3 and 2, in that order.
extern "C" void __cdecl Area18_ClearCells(void) {
    static constexpr unsigned char kRows[] = {0x4C, 0x4D, 3, 2};
    for (const unsigned char z : kRows) {
        AH_CALL(AreaMap_SetByte)(0x1F, z, 0);
        AH_CALL(AreaMap_SetByte)(0x20, z, 0);
    }
}

// ===========================================================================
// Area 19
// ===========================================================================

// original 0x402D60 (area 19's handler 0, Area19_Handlers 0x5E6B60; PSX
// 0x801F2C04): Camera_Distance + 0xA00 (a word, no bound), MapView_Redraw = 2.
extern "C" void __cdecl Area19_CameraOut(void) {
    Camera_Distance = static_cast<short>(Camera_Distance + 0xA00);
    MapView_Redraw = 2;
}

// original 0x402D80 (area 19's handler 1; PSX 0x801F2C2C): Camera_Distance -
// 0xA00, MapView_Redraw = 2.
extern "C" void __cdecl Area19_CameraIn(void) {
    Camera_Distance = static_cast<short>(Camera_Distance + 0xF600);
    MapView_Redraw = 2;
}

// original 0x402DA0 (area 19's handler 5; PSX 0x801F2CB4): Kind2_Place(4).
extern "C" void __cdecl Area19_PlaceKind2(void) { AH_CALL(Kind2_Place)(4); }

// ===========================================================================
// Area 20
// ===========================================================================

// original 0x402DB0 (area 20's init, the descriptor 0x5E6C58's +0x40; PSX
// 0x801F2D2C): a five-byte `jmp 0x402DC0`, then the body - Area29_PickFieldObject's
// (0x4037B0) instruction for instruction over area 20's own tables.
// Rand() & 0x3F picks one of the first eight field objects by the chances
// Area20_Weights 0x5E6C4C (byte-wide running subtraction; 8, none, when the
// roll is past them all); the other seven get +0 = 0. The kept one goes to
// Area20_Cells[Rand() & 7] (+0x34 = x << 16, +0x38 = z << 16, +0x3E =
// AreaMap_Elevation(+0x34, z << 16)). Last, either way, Field_EdgeBits = the
// low word of the leader's dword +0x134 - 5.
extern "C" void __cdecl Area20_PickFieldObject(void) {
    unsigned roll = static_cast<unsigned>(AH_CALL(Rand)()) & 0x3F;
    unsigned keep = 0;
    for (; keep < 8; ++keep) {
        const unsigned chance = At(at::kA20Weights + keep)[0];
        if (roll < chance) break;
        roll = (roll - chance) & 0xFF;
    }
    for (unsigned i = 0; i < 8; ++i)
        if (i != keep) Sprite_Objects[i * 0xA4] = 0;
    if (keep < 8) {
        const unsigned cell = static_cast<unsigned>(AH_CALL(Rand)()) & 7;
        unsigned char* const o = Sprite_Objects + keep * 0xA4;
        const U x = static_cast<U>(At(at::kA20Cells + cell * 2)[0]) << 16;
        const U z = static_cast<U>(At(at::kA20Cells + cell * 2 + 1)[0]) << 16;
        SetLong(o + 0x34, static_cast<std::int32_t>(x));
        SetLong(o + 0x38, static_cast<std::int32_t>(z));
        const long height = AH_CALL(AreaMap_Elevation)(Long(o + 0x34), static_cast<long>(z));
        SetWord(o + 0x3E, static_cast<unsigned>(height));
    }
    Field_EdgeBits = static_cast<unsigned short>(Long(At(at::kLeaderEdge)) - 5);
}

// ===========================================================================
// Area 21 (choice handlers)
// ===========================================================================

// original 0x402E80 (area 21's choice 1, the descriptor 0x5E7280's +0x34):
// choice not 0: message 0x18. Else the money 0x904058 below 0x14: message
// 0x17; else the money take (0x591BC0) of 0x14 and message 0x11.
extern "C" void __cdecl Area21_ChoicePay(void) {
    if (At(at::kChoice)[0] != 0) {
        SetMessage(0x18);
        return;
    }
    if (static_cast<U>(Long(At(at::kMoney))) < 0x14) {
        SetMessage(0x17);
        return;
    }
    AH_AT(unsigned char (__cdecl*)(unsigned, unsigned), area_w0b::kMoneyTake)(0x14, 0);
    SetMessage(0x11);
}

// original 0x402EC0 (area 21's choice 2): message = Area21_Messages 0x5E72C4
// by the s8 choice (unchecked).
extern "C" void __cdecl Area21_ChoiceMessage(void) { SetMessage(Word(At(at::kA21Messages + static_cast<U>(Choice() * 2)))); }

// original 0x402EE0 (area 21's choices 3 and 4). Choice not 0: the counter
// 0x90384B + 1, and at 8 back to 0 with message 0x34, else message 0x31.
// Choice 0: Inventory_Count(0, 3, 0) (its word) 0 - message 0x32; else n =
// (counter + 1) * 10 (10 for a counter of 0), Crt_sprintf(Text_Records row 1,
// "%d", n), message 0x30, the money give (0x591BE0) of n, the inventory take
// (0x591B60) of item 3 of category 0, one, and Flags_Set(the row 0x929ED0
// points at, 0x12).
extern "C" void __cdecl Area21_ChoiceTrade(void) {
    if (At(at::kChoice)[0] != 0) {
        const auto count = static_cast<unsigned char>(At(at::kCounter)[0] + 1);
        At(at::kCounter)[0] = count;
        if (count == 8) {
            At(at::kCounter)[0] = 0;
            SetMessage(0x34);
        } else {
            SetMessage(0x31);
        }
        return;
    }
    if (AH_CALL(Inventory_Count)(0, 3, 0) == 0) {
        SetMessage(0x32);
        return;
    }
    const unsigned counter = At(at::kCounter)[0];
    const unsigned n = counter != 0 ? (counter + 1) * 10 : 10;
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kTextRow1)), reinterpret_cast<const char*>(At(at::kFormat)), n);
    SetMessage(0x30);
    AH_AT(unsigned char (__cdecl*)(unsigned, unsigned), area_w0b::kMoneyGive)(n, 0);
    AH_AT(unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned), area_w0b::kInventoryTake)(0, 3, 1, 0);
    AH_CALL(Flags_Set)(Pointer(at::kFlagBase), 0x12);
}

// ===========================================================================
// Area 22
// ===========================================================================

namespace {
// Areas 22's and 26's choice pattern: message 0xFFFF (none), then the script
// variable 0x903848 = `yes` for choice 0, `no` for choice 1, else unchanged.
void ChoiceSetVar(unsigned char yes, unsigned char no) {
    const signed char choice = Choice();
    SetMessage(0xFFFF);
    if (choice == 0) At(at::kScriptVar)[0] = yes;
    else if (choice == 1) At(at::kScriptVar)[0] = no;
}
}  // namespace

// original 0x402F90 (area 22's choice 0, the descriptor 0x5E7DE8's +0x34):
// no message; the variable 0x1E (choice 0) or 0x32 (1).
extern "C" void __cdecl Area22_ChoiceA(void) { ChoiceSetVar(0x1E, 0x32); }
// original 0x402FC0 (area 22's choice 1): 0x3C or 0x46.
extern "C" void __cdecl Area22_ChoiceB(void) { ChoiceSetVar(0x3C, 0x46); }
// original 0x402FF0 (area 22's choice 2): 0xB or 0x14.
extern "C" void __cdecl Area22_ChoiceC(void) { ChoiceSetVar(0xB, 0x14); }

// original 0x403020 (area 22's handler 0; PSX 0x801F2CC4): cells (3, 7), (3,
// 8), (4, 7), (4, 8) to 0.
extern "C" void __cdecl Area22_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(3, 7, 0);
    AH_CALL(AreaMap_SetByte)(3, 8, 0);
    AH_CALL(AreaMap_SetByte)(4, 7, 0);
    AH_CALL(AreaMap_SetByte)(4, 8, 0);
}

// original 0x403050 (one body for nine areas' choice or handler entries: 1,
// 23, 32, 40, 41, 108, 117 (twice), 141, 143; it lies in area 22's block):
// message 0xFFFF; choice 0 (the byte read before that store) arms the tail
// kind 0xA - 0x9039F3 = 0xA, 0x9039F4 = 5, 0x9039F5 = 0xFF.
extern "C" void __cdecl Area22_ArmTailOnYes(void) {
    const unsigned char choice = At(at::kChoice)[0];
    SetMessage(0xFFFF);
    if (choice != 0) return;
    At(at::kMsgMode)[0] = 0xA;
    At(at::kMsgState)[0] = 5;
    At(at::kMsgArg)[0] = 0xFF;
}

// ===========================================================================
// Area 23
// ===========================================================================

// original 0x403080 (area 23's choice 2 and handler 0; PSX 0x801F2C88): cells
// (0x48, 0xB), (0x49, 0xB), (0x48, 0xC), (0x49, 0xC) to 0.
extern "C" void __cdecl Area23_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(0x48, 0xB, 0);
    AH_CALL(AreaMap_SetByte)(0x49, 0xB, 0);
    AH_CALL(AreaMap_SetByte)(0x48, 0xC, 0);
    AH_CALL(AreaMap_SetByte)(0x49, 0xC, 0);
}

// original 0x4030B0 (area 23's choice 3 and handler 1; PSX 0x801F2CE0): the
// same four cells back - row 0xB to 0xC0, row 0xC to 0xA1.
extern "C" void __cdecl Area23_SetCells(void) {
    AH_CALL(AreaMap_SetByte)(0x48, 0xB, 0xC0);
    AH_CALL(AreaMap_SetByte)(0x49, 0xB, 0xC0);
    AH_CALL(AreaMap_SetByte)(0x48, 0xC, 0xA1);
    AH_CALL(AreaMap_SetByte)(0x49, 0xC, 0xA1);
}

// original 0x4030F0 (area 23's choice 4 and handler 2; PSX 0x801F2D38):
// Camera_ShiftY - 0x12, MapView_Redraw = 2.
extern "C" void __cdecl Area23_CameraUp(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0x12);
    MapView_Redraw = 2;
}

// original 0x403100 (area 23's choice 5 and handler 3; PSX 0x801F2D60):
// Camera_ShiftY + 0x12, MapView_Redraw = 2.
extern "C" void __cdecl Area23_CameraDown(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0x12);
    MapView_Redraw = 2;
}

// ===========================================================================
// Areas 24 and 25
// ===========================================================================

// original 0x403110 (area 24's choices 0 and 1): message 0xFFFF.
extern "C" void __cdecl Area24_NoMessage(void) { SetMessage(0xFFFF); }

// original 0x403120 (area 25's handler 0; PSX 0x801F2C04): on frames whose
// Frame_Counter & 3 is 0, Effect_FindFree to 0x903850; a slot (not 0xFF):
// its Effect_Objects record (the byte read back) +0 = 1, +5 = 0xB, the dword
// +0x1C = 6, +7 = 1 while the byte 0x90384A is below 0x14, else 3.
extern "C" void __cdecl Area25_SpawnEffect(void) {
    if ((Frame_Counter & 3) != 0) return;
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    At(at::kFoundSlot)[0] = slot;
    if (slot == 0xFF) return;
    const unsigned count = At(at::kEffectCount)[0];
    unsigned char* const record = Effect_Objects + static_cast<U>(At(at::kFoundSlot)[0]) * 0x80u;
    record[0] = 1;
    record[5] = 0xB;
    SetLong(record + 0x1C, 6);
    record[7] = count < 0x14 ? 1 : 3;
}

// ===========================================================================
// Area 26
// ===========================================================================

// original 0x403180 (area 26's choice 0, the descriptor 0x5EC5A0's +0x34):
// message = Area26_Messages 0x5EC5E4 by the s8 choice (unchecked); the
// variable 0xBE (choice 0) or 0xBF (1).
extern "C" void __cdecl Area26_ChoiceMessage(void) {
    const signed char choice = Choice();
    SetMessage(Word(At(at::kA26Messages + static_cast<U>(choice * 2))));
    if (choice == 0) At(at::kScriptVar)[0] = 0xBE;
    else if (choice == 1) At(at::kScriptVar)[0] = 0xBF;
}

// original 0x4031B0 (area 26's choice 1): no message; the variable 8 or 0xF.
extern "C" void __cdecl Area26_ChoiceSetVar(void) { ChoiceSetVar(8, 0xF); }

namespace {
unsigned char* FlagRow() { return Pointer(at::kFlagBase); }
}  // namespace

// original 0x4031E0 (area 26's choice 2 and handler 0; PSX 0x801F309C):
// Flags_Set(row, 0x29), Flags_Clear(row read again, 0x28) - the row the
// pointer 0x929ED0 holds.
extern "C" void __cdecl Area26_Flag29Set28Clear(void) {
    AH_CALL(Flags_Set)(FlagRow(), 0x29);
    AH_CALL(Flags_Clear)(FlagRow(), 0x28);
}
// original 0x403200 (choice 3, handler 1; PSX 0x801F30D4): Flags_Clear 0x29.
extern "C" void __cdecl Area26_Flag29Clear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x29); }
// original 0x403220 (choice 4, handler 2; PSX 0x801F30FC): Flags_Set 0x28.
extern "C" void __cdecl Area26_Flag28Set(void) { AH_CALL(Flags_Set)(FlagRow(), 0x28); }
// original 0x403240 (choice 5, handler 3; PSX 0x801F3124): Flags_Clear 0x28.
extern "C" void __cdecl Area26_Flag28Clear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x28); }
// original 0x403260 (choice 6, handler 4; PSX 0x801F314C): Flags_Set 0x2B.
extern "C" void __cdecl Area26_Flag2BSet(void) { AH_CALL(Flags_Set)(FlagRow(), 0x2B); }
// original 0x403280 (choice 7, handler 5; PSX 0x801F3184): Flags_Clear 0x2B.
extern "C" void __cdecl Area26_Flag2BClear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x2B); }
// original 0x4032A0 (choice 8, handler 6; PSX 0x801F31AC): Flags_Set 0x2C.
extern "C" void __cdecl Area26_Flag2CSet(void) { AH_CALL(Flags_Set)(FlagRow(), 0x2C); }
// original 0x4032C0 (choice 9, handler 7; PSX 0x801F31D4): Flags_Clear 0x2C.
extern "C" void __cdecl Area26_Flag2CClear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x2C); }

// original 0x4032E0 (choice 10, handler 8; PSX 0x801F31FC): Inventory_Add(4,
// 3, 1) (a fourth word 0 pushed and not read), Sound_PlayEffect(0x106),
// Flags_Set(row, 0x33).
extern "C" void __cdecl Area26_GiveItem(void) {
    AH_CALL(Inventory_Add)(4, 3, 1);
    AH_CALL(Sound_PlayEffect)(0x106);
    AH_CALL(Flags_Set)(FlagRow(), 0x33);
}

// original 0x403310 (choice 13, handler 11; PSX 0x801F32C8): Camera_ShiftY =
// 0, MapView_Redraw = 2, Music_FadeOutStop(0xA).
extern "C" void __cdecl Area26_ResetCamera(void) {
    Camera_ShiftY = 0;
    MapView_Redraw = 2;
    AH_CALL(Music_FadeOutStop)(0xA);
}

// original 0x403330 (choice 14, handler 12; PSX 0x801F3348): as
// Area11_SpawnEffect with its own table - the leader's words +0x2E / +0x30
// read, Sprite_Current made the leader, Effect_Spawn(1, 0,
// Area26_EffectKinds[the first party list's first member], x, z); an answer
// other than 0xFF to Sprite_Current[0xB] (read again). The member id indexes
// the table unchecked.
extern "C" void __cdecl Area26_SpawnEffect(void) {
    const auto z = static_cast<short>(Word(At(at::kLeader + 0x30)));
    const auto x = static_cast<short>(Word(At(at::kLeader + 0x2E)));
    const unsigned member = At(at::kPartyFirst)[0];
    Sprite_Current = At(at::kLeader);
    const unsigned char kind = At(at::kA26EffectKinds + member)[0];
    const unsigned char slot = AH_CALL(Effect_Spawn)(1, 0, static_cast<signed char>(kind), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// original 0x403380 (choice 15, handler 13; PSX 0x801F3370): Flags_Set 0x2E.
extern "C" void __cdecl Area26_Flag2ESet(void) { AH_CALL(Flags_Set)(FlagRow(), 0x2E); }

// original 0x4033A0 (choice 16, handler 14): Effect_FindFree; a slot (not
// 0xFF): its Effect_Objects record +0x38 = 0x68000, +0x34 = 0x70000, +0 = 1,
// +5 = 0x33, then +0x3C = AreaMap_Elevation(0x70000, 0x68000) as a signed
// word << 16.
extern "C" void __cdecl Area26_PlaceEffect(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const record = Effect_Objects + static_cast<U>(slot) * 0x80u;
    SetLong(record + 0x38, 0x68000);
    SetLong(record + 0x34, 0x70000);
    record[0] = 1;
    record[5] = 0x33;
    const long height = AH_CALL(AreaMap_Elevation)(Long(record + 0x34), Long(record + 0x38));
    SetLong(record + 0x3C, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(height)) << 16));
}

// ===========================================================================

void AreaW0b_Inject() {
    if (bof3::WantsShadow("area_w0b")) area_w0b::SelfTest();
    BOF3_INJECT(Area16_PlaceMessage);
    BOF3_INJECT(Area16_PlateRun);
    BOF3_INJECT(Area16_PlateStart);
    BOF3_INJECT(Area16_PlateShow);
    BOF3_INJECT(Area16_PlateGrow);
    BOF3_INJECT(Area16_PlateHold);
    BOF3_INJECT(Area16_PlateShrink);
    BOF3_INJECT(Area16_HudRun);
    BOF3_INJECT(Area16_HudFrame);
    BOF3_INJECT(Area16_FrameStep);
    BOF3_INJECT(Area16_FrameSlideIn);
    BOF3_INJECT(Area16_FrameHold);
    BOF3_INJECT(Area16_FrameSlideOut);
    BOF3_INJECT(Area16_BoxStep);
    BOF3_INJECT(Area16_BoxSlideIn);
    BOF3_INJECT(Area16_BoxHold);
    BOF3_INJECT(Area16_BoxSlideOut);
    BOF3_INJECT(Area16_DrawFrame);
    BOF3_INJECT(Area16_DrawSprite);
    BOF3_INJECT(Area16_DrawHud);
    BOF3_INJECT(Area16_Record8Run);
    BOF3_INJECT(Area16_Record8Place);
    BOF3_INJECT(Area16_Record4Run);
    BOF3_INJECT(Area16_Record4MarkCell);
    BOF3_INJECT(Area16_DrawDrift);
    BOF3_INJECT(Area18_SetRecordByte);
    BOF3_INJECT(Area18_SkipScript);
    BOF3_INJECT(Area18_ClearCells);
    BOF3_INJECT(Area19_CameraOut);
    BOF3_INJECT(Area19_CameraIn);
    BOF3_INJECT(Area19_PlaceKind2);
    BOF3_INJECT(Area20_PickFieldObject);
    BOF3_INJECT(Area21_ChoicePay);
    BOF3_INJECT(Area21_ChoiceMessage);
    BOF3_INJECT(Area21_ChoiceTrade);
    BOF3_INJECT(Area22_ChoiceA);
    BOF3_INJECT(Area22_ChoiceB);
    BOF3_INJECT(Area22_ChoiceC);
    BOF3_INJECT(Area22_ClearCells);
    BOF3_INJECT(Area22_ArmTailOnYes);
    BOF3_INJECT(Area23_ClearCells);
    BOF3_INJECT(Area23_SetCells);
    BOF3_INJECT(Area23_CameraUp);
    BOF3_INJECT(Area23_CameraDown);
    BOF3_INJECT(Area24_NoMessage);
    BOF3_INJECT(Area25_SpawnEffect);
    BOF3_INJECT(Area26_ChoiceMessage);
    BOF3_INJECT(Area26_ChoiceSetVar);
    BOF3_INJECT(Area26_Flag29Set28Clear);
    BOF3_INJECT(Area26_Flag29Clear);
    BOF3_INJECT(Area26_Flag28Set);
    BOF3_INJECT(Area26_Flag28Clear);
    BOF3_INJECT(Area26_Flag2BSet);
    BOF3_INJECT(Area26_Flag2BClear);
    BOF3_INJECT(Area26_Flag2CSet);
    BOF3_INJECT(Area26_Flag2CClear);
    BOF3_INJECT(Area26_GiveItem);
    BOF3_INJECT(Area26_ResetCamera);
    BOF3_INJECT(Area26_SpawnEffect);
    BOF3_INJECT(Area26_Flag2ESet);
    BOF3_INJECT(Area26_PlaceEffect);
}
