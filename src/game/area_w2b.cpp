// World 2's areas 85..88: the code of the PSX's BIN/WORLD02/AREA085..088.EMI
// compiled into the exe at 0x40F720..0x411EFF - 66 functions (the band's 68
// less WorldMap_PinSprite 0x4112A0, round seven's, and WorldMap_FrameWait
// 0x411310, round eight's, which lie in area 88's block), each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). Round ten group AR2B; docs/area_w2b.md has the
// areas one section each.
//
// Area 85: ten handlers (a kind-2 walk, an object that follows party member
// 2, two effect-0x47 spawns, a Field_Slots script start and release, a height
// nudge, a sound) and an init that darkens CLUT row 6 through a brightness
// shift an engine effect state (0x478649) also calls. Area 86: a cell hook
// whose three floor switches toggle story flags 8..10 unless a party member
// stands on the switch's gate cells, and an init that closes two map cells
// before chapter 13. 0x40FC40 is areas 65's and 87's init (in area 86's
// block by address). Areas 87 and 88 are world maps (WorldMap_Records records
// 4 and 5): area 45's code (docs/area_w1b.md section 5, itself area 16's)
// instruction for instruction over their own tables - area 87 with two
// constants different (the plate's animation bank 0x156, the region label's
// offset cell 0x803580), area 88 with the same two (bank 0x157) and a place
// hook whose name sets hold eleven items and fill eleven text rows where
// area 45's hold four. So both copies are one body of code here, over a
// WorldMapTables each (area_w2b_callees.h), with a named entry per address.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through an area's .data state table abort past the table where
// the original would call whatever the next dwords hold, and area 85's effect
// spawns abort on a slot past Effect_Objects' twenty (the owner's rule for an
// unchecked index, round9 doc section 6; no route reaches either). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/area_w2b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w2b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2b::at;
using U = std::uint32_t;
using area_harness::Handler;
using at::WorldMapTables;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char* Bank() { return At(at::kStoryFlags); }
unsigned char* PartyRecord(U index) { return At(at::kLeader + index * at::kPartyStride); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, const char* what, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s_%s: state %u is past its %u-entry table 0x%X", who, what, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

// Area 85's shift of CLUT row 6, called by its init by the original address.
constexpr U kClutShift85 = bof3::addr::Area85_ClutShift;
// Area 86's member test, called by its cell hook.
constexpr U kMemberNear86 = bof3::addr::Area86_MemberNear;
using ShiftFn = void (__cdecl*)(int);
using NearFn = unsigned char (__cdecl*)(unsigned, unsigned);
using ObjectFn = void (__cdecl*)(unsigned char*);
using SlotFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*);
using BitsFn = void (__cdecl*)(unsigned char*, unsigned);
using ValueFn = void (__cdecl*)(unsigned);

// A world-map copy's own functions another of them calls directly.
using DrawAt = void (__cdecl*)(int, int);
using DrawSpriteAt = void (__cdecl*)(int, int, unsigned);

}  // namespace

// ===========================================================================
// Area 85
// ===========================================================================

// original 0x40F720 (area 85's handler 0, Area85_Handlers 0x61177C): the
// kind-2 object's step count MoveScript_Object[7] = (Sprite_Current +0x34 -
// 0x160000) >> 15 (arithmetic, a byte); MoveCmd_MoveKind2(7); then
// Sprite_Current (read again) +0x14 = 0, MoveScript_FAWord = 0,
// MoveScript_Object (read again) +0 |= 0x40, Field_Kind2Z = 0x3E0000 and
// Sprite_Current +0x38 = 0x3E0000.
extern "C" void __cdecl Area85_Kind2Walk(void) {
    MoveScript_Object[7] = static_cast<unsigned char>(static_cast<U>((Long(Cur() + 0x34) - 0x160000) >> 15));
    AH_CALL(MoveCmd_MoveKind2)(7);
    SetLong(Cur() + 0x14, 0);
    MoveScript_FAWord = 0;
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Field_Kind2Z = 0x3E0000;
    SetLong(Cur() + 0x38, 0x3E0000);
}

// original 0x40F780 (area 85's handler 1): in run 6 step 0xC (MoveScript_Var7
// and the byte 0x8034E5): Sprite_Current +0 bit 6 cleared,
// Field_MemberSprite(2, 2), Sprite_SetAnimation(1). Otherwise, for each
// party record 0 .. Field_MemberCount - 1 (the count a byte, read again each
// turn, unchecked against the three records): the record whose +0x89 is 2
// has its +0x34 / +0x38 / +0x3C copied to Sprite_Current's (read again for
// each); then the script word MoveScript_Object +0xA - 2 (the op runs again
// next frame).
extern "C" void __cdecl Area85_FollowMember2(void) {
    if (At(at::kScriptVar7)[0] == 6 && At(at::kScriptStep)[0] == 0xC) {
        Cur()[0] = static_cast<unsigned char>(Cur()[0] & 0xBF);
        AH_CALL(Field_MemberSprite)(2, 2);
        AH_CALL(Sprite_SetAnimation)(1);
        return;
    }
    for (unsigned char i = 0; i < Field_MemberCount; ++i) {
        const unsigned char* const record = PartyRecord(i);
        if (record[0x89] != 2) continue;
        SetLong(Cur() + 0x34, Long(record + 0x34));
        SetLong(Cur() + 0x38, Long(record + 0x38));
        SetLong(Cur() + 0x3C, Long(record + 0x3C));
    }
    move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 0xFFFEu);
}

// original 0x40F830 (area 85's handler 2): Sprite_Current +6 = 7,
// Field_ActiveMember +0x9E = 0.
extern "C" void __cdecl Area85_SetType7(void) {
    Cur()[6] = 7;
    Field_ActiveMember[0x9E] = 0;
}

namespace {
// Area 85's handlers 3 and 4 (one shape, the effect's +0xB 1 or 0):
// Sprite_Current (read after the call) +0xB = Effect_FindFree's slot. None
// (0xFF): the script word MoveScript_Object +0xA - 2 (the op runs again).
// Else the slot's record (the slot re-read from +0xB for each store) +0 = 1,
// kind +5 = 0x47, +0xB = `sub`; Sound_PlayEffect(0x200). Ours aborts on a
// slot past Effect_Objects' twenty records (Effect_FindFree answers 0..19).
void SpawnEffect47(const char* who, unsigned char sub) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    unsigned char* const o = Cur();
    o[0xB] = slot;
    if (o[0xB] == 0xFF) {
        move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 0xFFFEu);
        return;
    }
    const auto record = [who, o]() {
        if (o[0xB] >= 20) bof3::Fatal("%s: effect slot %u is past Effect_Objects' 20 records", who, static_cast<unsigned>(o[0xB]));
        return Effect_Objects + o[0xB] * at::kEffectStride;
    };
    record()[0] = 1;
    record()[5] = 0x47;
    record()[0xB] = sub;
    AH_CALL(Sound_PlayEffect)(0x200);
}
}  // namespace

// original 0x40F850 (area 85's handler 3): SpawnEffect47 with +0xB = 1.
extern "C" void __cdecl Area85_SpawnEffect47A(void) { SpawnEffect47("Area85_SpawnEffect47A", 1); }

// original 0x40F8C0 (area 85's handler 4): SpawnEffect47 with +0xB = 0.
extern "C" void __cdecl Area85_SpawnEffect47B(void) { SpawnEffect47("Area85_SpawnEffect47B", 0); }

// original 0x40F930 (area 85's handler 5): Sprite_Current +0x24 bit 0
// cleared; Sprite_SetAnimationBank(0x184), Sprite_SetAnimation(0); then
// +0x2A = 1 and +0 |= 0x10 (Sprite_Current read again for each).
extern "C" void __cdecl Area85_SetUpObject(void) {
    Cur()[0x24] = static_cast<unsigned char>(Cur()[0x24] & 0xFE);
    AH_CALL(Sprite_SetAnimationBank)(0x184);
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[0x2A] = 1;
    Cur()[0] = static_cast<unsigned char>(Cur()[0] | 0x10);
}

// original 0x40F960 (area 85's handler 6): 0x454A80(Sprite_Current) - its
// Field_Slots scripts released; 0x455290(Sprite_Current, read again, the
// script 0x611814) - a new one started (its slot not read);
// Sprite_SetAnimation(0).
extern "C" void __cdecl Area85_StartSlotScript(void) {
    AH_AT(ObjectFn, area_w2b::kSlotsReleaseFor)(Cur());
    AH_AT(SlotFn, area_w2b::kSlotStart)(Cur(), At(at::kA85SlotScript));
    AH_CALL(Sprite_SetAnimation)(0);
}

// original 0x40F990 (area 85's handler 7, and area 198's handler 3: one body,
// in area 85's block): 0x454A80(Sprite_Current) - the object's Field_Slots
// scripts released.
extern "C" void __cdecl Area85_ReleaseSlots(void) { AH_AT(ObjectFn, area_w2b::kSlotsReleaseFor)(Cur()); }

// original 0x40F9A0 (area 85's handler 8): the word Sprite_Current +0x3E +=
// 0x18.
extern "C" void __cdecl Area85_Raise18(void) { SetWord(Cur() + 0x3E, Word(Cur() + 0x3E) + 0x18u); }

// original 0x40F9B0 (area 85's handler 9): Sound_PlayEffect(0x201) unless
// Field_Request is 5.
extern "C" void __cdecl Area85_Sound201(void) {
    if (Field_Request != 5) AH_CALL(Sound_PlayEffect)(0x201);
}

// original 0x40F9D0 (area 85's init, its descriptor 0x6117A8's +0x40):
// Area85_ClutShift(-8).
extern "C" void __cdecl Area85_Init(void) { AH_AT(ShiftFn, kClutShift85)(-8); }

// original 0x40F9E0 (called by Area85_Init with -8, and by engine code at
// 0x478649 - an effect state's frame, with its record's dword +0x10): for
// each of the 256 words of CLUT row 6 as loaded (0x80C180, Gfx_ClutStripSource
// + 0xC00), each of its three 5-bit channels that is not 0 moved by `delta`
// and held to 1..0x1F (0 at or below 0; 32-bit signed arithmetic), bit 15
// kept; the word to the live strip's row 6 (0x810180, Gfx_ClutStrip +
// 0xC00). Then Gfx_ClutStripDirty = 1.
extern "C" void __cdecl Area85_ClutShift(int delta) {
    const auto channel = [delta](U c) -> U {
        if (c == 0) return 0;
        const auto v = static_cast<std::int32_t>(c + static_cast<U>(delta));
        if (v <= 0) return 0;
        return v > 0x1F ? 0x1Fu : static_cast<U>(v);
    };
    for (U n = 0; n < 0x100; ++n) {
        const U word = Word(At(at::kClutRow6 + n * 2));
        const U r = channel(word & 0x1F);
        const U g = channel((word >> 5) & 0x1F);
        const U b = channel((word >> 10) & 0x1F);
        SetWord(At(at::kClutRow6Live + n * 2), (((b << 5) | g) << 5) | (word & 0x8000) | r);
    }
    Gfx_ClutStripDirty = 1;
}

// ===========================================================================
// Area 86
// ===========================================================================

// original 0x40FA90 (Area_CellHooks' entry for area 86, (x, z) cell bytes
// through Area_CellHook): the first of Area86_Switches' three (x, z, facing,
// flag) records whose x and z are the arguments' low bytes and whose facing
// is the leader's +8. None: al 0. Else, for every cell of the switch's
// rectangle in Area86_Rects (x from x0 while below x1, z from z0 while below
// z1, the far bounds read again each turn) Area86_MemberNear(x, z): a member
// there (an answer 0..0x7F) is al 0. Then with story flag 0x1C set, al 0;
// else 0x57C160 toggles the switch's flag, Sound_PlayEffect(0x200),
// 0x469FE0(0xF) (an effect of kind 4, which sets flag 0x1C), al 1.
extern "C" unsigned char __cdecl Area86_SwitchHook(unsigned x, unsigned z) {
    U k = 0;
    for (U a = at::kA86Switches + 1; a < at::kA86SwitchesEnd + 1; a += 4, ++k) {
        if (At(a - 1)[0] == (x & 0xFF) && At(a)[0] == (z & 0xFF) && At(a + 1)[0] == At(at::kLeaderDir)[0]) break;
    }
    if (k == 3) return 0;
    const unsigned char* const rect = At(at::kA86Rects + k * 4);
    for (std::int32_t cx = rect[0]; cx < rect[2]; ++cx) {
        for (std::int32_t cz = rect[1]; cz < rect[3]; ++cz) {
            const auto near = static_cast<signed char>(AH_AT(NearFn, kMemberNear86)(static_cast<unsigned>(cx), static_cast<unsigned>(cz)));
            if (near >= 0) return 0;
        }
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x1C) != 0) return 0;
    AH_AT(BitsFn, area_w2b::kFlagsToggle)(Bank(), At(at::kA86Switches + 3 + k * 4)[0]);
    AH_CALL(Sound_PlayEffect)(0x200);
    AH_AT(ValueFn, area_w2b::kSpawnKind4)(0xF);
    return 1;
}

// original 0x40FB70 (called by Area86_SwitchHook): the first party record 0
// .. Field_MemberCount - 1 (a byte, unchecked against the three records)
// whose +0 is not 0 and whose position a step ahead - (dword +0xC) * (byte
// +9) + (dword +0x34), less x << 16 - and the same in z with +0x10 / +0x38
// are each, as absolute values, below ((byte +0x70) + 2) << 15: its index in
// al. None: al 0xFF. 32-bit wrapping arithmetic throughout, as the original's
// (an absolute value of 0x80000000 stays negative and passes).
extern "C" unsigned char __cdecl Area86_MemberNear(unsigned x, unsigned z) {
    const std::int32_t count = Field_MemberCount;
    for (std::int32_t i = 0; i < count; ++i) {
        const unsigned char* const record = PartyRecord(static_cast<U>(i));
        if (record[0] == 0) continue;
        const U steps = record[9];
        const auto reach = static_cast<std::int32_t>((static_cast<U>(record[0x70]) + 2) << 15);
        const auto dx = static_cast<std::int32_t>(static_cast<U>(Long(record + 0xC)) * steps - (x << 16) + static_cast<U>(Long(record + 0x34)));
        const auto ax = static_cast<std::int32_t>(dx < 0 ? 0u - static_cast<U>(dx) : static_cast<U>(dx));
        if (ax >= reach) continue;
        const auto dz = static_cast<std::int32_t>(static_cast<U>(Long(record + 0x10)) * steps - (z << 16) + static_cast<U>(Long(record + 0x38)));
        const auto az = static_cast<std::int32_t>(dz < 0 ? 0u - static_cast<U>(dz) : static_cast<U>(dz));
        if (az < reach) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x40FC10 (area 86's init, its descriptor 0x611C38's +0x40): while
// the s8 chapter byte Cond_ByteFA is below 0xD, AreaMap_SetByte(0xB, 9, 0x50)
// and AreaMap_SetByte(0xB, 0xA, 0x50).
extern "C" void __cdecl Area86_Init(void) {
    if (Cond_ByteFA >= 0xD) return;
    AH_CALL(AreaMap_SetByte)(0xB, 9, 0x50);
    AH_CALL(AreaMap_SetByte)(0xB, 0xA, 0x50);
}

// original 0x40FC40 (the init of areas 65 and 87 - their descriptors'
// +0x40 - in area 86's block by address): when the party came from area 0x3C
// (the word 0x802290), Field_ScriptFlags bit 13 cleared.
extern "C" void __cdecl Area87_Init(void) {
    if (Word(At(at::kPrevArea)) == 0x3C) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
}

// ===========================================================================
// Areas 87 and 88: the world-map copies (area 45's code, docs/area_w1b.md
// section 5; area 16's in docs/area_w0b.md; area 33's in
// docs/worldmap_area.md sections 1..4)
// ===========================================================================

namespace {

// The region box's leave test the HUD machine's states share: the map's mode
// byte set while +0xB is, or Field_Request 2, or bit 8 of Field_ScriptFlags.
bool BoxLeaves(const unsigned char* o) {
    if (At(at::kMapMode)[0] != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

// The first of `count` button-table entries whose mask word has a bit of the
// button word's low 16 bits: its sprite byte, or -1.
int KeyIndex(const WorldMapTables& t, U button_word, U count) {
    for (U k = 0; k < count; ++k) {
        const unsigned char* const entry = At(t.buttons + k * 4);
        if ((Word(entry) & button_word & 0xFFFF) != 0) return entry[2];
    }
    return -1;
}

// The field hook (WorldMap_FieldHooks; 0x56DE30 calls the entry of
// WorldMap_RecordIndex): a two-state machine on the s8 0x9039F4.
//   0: ScriptFlags_Set40; the cell the leader stands on (the high words of its
//      +0x34 / +0x38) asked of AreaMap_ByteAt. 0xA1 (a place): the message of
//      row (the first place row whose word is the place 0x937F82, the row
//      count when none) * 16 + (s8) Cond_ByteFA, Msg_OpenScript. Any other:
//      the cell record whose (x, z) bytes are the leader's cell words (read
//      again after the call; searched with NO bound), its id byte +3; the
//      name set whose id it is (3 when none); up to set_stride - 1 Text_Records
//      rows from its item bytes (0xFF ends them; the rows end at
//      text_rows_end): "????????" and a 0 for an item whose byte at 0x9040EC
//      is 0, else the item's 16-byte name (0x669CD8 for item 0x16, else
//      Item_NamePtr(0, item + 0x38)); Msg_OpenScript(set + 0x16). Then
//      0x9039F4 (read again) + 1 and Field_Request = 2.
//   1: once Field_Request is not 2, ScriptFlags_Clear40 and the three bytes
//      0x9039F3..0x9039F5 zeroed.
// Any other state does nothing. As the original: neither the row, the byte
// nor the set 3 (which reads the plate state table's bytes) is checked.
void PlaceMessage(const WorldMapTables& t) {
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
        for (U a = t.place_messages; a < t.place_messages_end; a += 0x20, ++row)
            if (Word(At(a)) == place) break;
        const std::int32_t index = (row << 4) + Cond_ByteFA;
        AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(t.place_messages + static_cast<U>(index * 2)))));
    } else {
        const unsigned cx = Word(At(at::kLeaderCellWordX));
        const unsigned cz = Word(At(at::kLeaderCellWordZ));
        U cell = t.cells;
        while (!(At(cell)[0] == cx && At(cell)[1] == cz)) cell += 4;
        const unsigned id = At(cell)[3];
        U set = 0;
        for (U a = t.name_sets; a < t.name_sets_end; a += t.set_stride, ++set)
            if (At(a)[0] == id) break;
        const unsigned char* items = At(t.name_sets + 1 + set * t.set_stride);
        for (U row = at::kTextRecords; row < t.text_rows_end; row += 0x20, ++items) {
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

// The place plate (the record's +0: effect kind 0). The cell ahead of the
// leader - the high words of (leader +9) * (+0xC) + (+0x34) and of the same
// with +0x10 / +0x38 - to +0xC / +0x10 (sign-extended), the kind +0xB by
// AreaMap_ByteAt(x, z) asked up to three times: 0xA1 1, 0xA0 2, 0xAE 3, else
// Field_ScriptFlags2 bit 12 4, else 0; then the plate state table by +1
// (read after the calls; ours aborts past its five entries).
void PlateRun(const WorldMapTables& t) {
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
    StateEntry(t.name, "PlateRun", t.plate_states, 5, Cur()[1])();
}

// The plate's state 0: +0x24 = 0x80, Sprite_SetAnimationBank(the copy's
// bank), then +0x29 = 5, +0x2A, +0x5D, +0x5E, +0x5F = 0 and +1 = 1 (the
// show), Sprite_Current read again for each store.
void PlateStart(const WorldMapTables& t) {
    Cur()[0x24] = 0x80;
    AH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(t.plate_bank));
    Cur()[0x29] = 5;
    Cur()[0x2A] = 0;
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    Cur()[1] = 1;
}

// The plate's state 1: +7 = +0xB. Kind 1: the plate animation entry whose
// word is the place 0x937F82 - searched with NO bound - +0x18 = the place
// (zero-extended), its animation byte. Kinds 2, 3, 4: animations 3, 0, 1.
// Each: +0x40 = 0, +0x44 = 0x10000, +0x48 = 2, +9 = 8, Sprite_SetAnimation,
// +1 = 2. Other kinds: no more.
void PlateShow(const WorldMapTables& t) {
    Cur()[7] = Cur()[0xB];
    const unsigned kind = Cur()[0xB];
    unsigned animation;
    if (kind == 1) {
        const unsigned place = Word(At(at::kPlace));
        U entry = t.plate_anims;
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

// The plate's state 2: WorldMap_PinSprite; +0x40 += 0x2000; +9 - 1, at 0
// +0x48 = 0 and +1 = 3; then a tail jump to Sprite_QueueOverlay.
void PlateGrow() {
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

// The plate's state 3: WorldMap_PinSprite; unless Game_Mode is 1 the plate
// leaves (+0x48 = 2, +9 = 8, +1 = 4) when +0xB is not +7, or Field_Request
// is 5, or +7 is 1 and +0x18 is not the place (zero-extended); a tail jump to
// Sprite_QueueOverlay.
void PlateHold() {
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

// The plate's state 4: WorldMap_PinSprite; +0x40 -= 0x2000; +9 - 1. Not 0:
// a tail jump to Sprite_QueueOverlay. At 0: Field_Request 5 a tail jump to
// Effect_Release, else +0x48 = 0 and +1 = 1.
void PlateShrink() {
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

// The dial frame's slide, state 1: the word +0x2E += 0x10; at 0x10 and above
// (s16) +2 + 1; then a tail jump to the copy's FrameHold.
void FrameSlideIn(const WorldMapTables& t) {
    SetWord(Cur() + 0x2E, Word(Cur() + 0x2E) + 0x10u);
    unsigned char* const o = Cur();
    if (S16(o + 0x2E) >= 0x10) o[2] = static_cast<unsigned char>(o[2] + 1);
    area_harness::Phase(t.fn_frame_hold)();
}

// State 2: the mode byte 2 makes +2 = 3; the copy's DrawFrame(0x10, the word
// +0x2E). The y pushed carries a stale high half the drawing never reads:
// ours passes the word sign-extended.
void FrameHold(const WorldMapTables& t) {
    if (At(at::kMapMode)[0] == 2) Cur()[2] = 3;
    AH_AT(DrawAt, t.fn_draw_frame)(0x10, S16(Cur() + 0x2E));
}

// State 3: the word +0x2E -= 0x10; at -0x30 and below +2 = 0; unless the
// mode byte is 2, +2 = 1 (over the 0); DrawFrame(0x10, y).
void FrameSlideOut(const WorldMapTables& t) {
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
    AH_AT(DrawAt, t.fn_draw_frame)(0x10, S16(o + 0x2E));
}

// The region box, state 1: y +0x30 -= 10; at 0xC8 and below (s16) +3 + 1;
// when the box leaves +3 = 3; the copy's DrawHud(0x5C, y) - the y pushed
// with the object pointer's high half above the word, which the drawing
// never reads.
void BoxSlideIn(const WorldMapTables& t) {
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
    AH_AT(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// State 2: while +0xB is 0 +9 + 1, at 0x5A and above (u8) +0xB + 1; when the
// box leaves +3 + 1; DrawHud(0x5C, y).
void BoxHold(const WorldMapTables& t) {
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
    AH_AT(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// State 3: y +0x30 += 10; at 0xF0 and above (s16) +3 = 0; unless the mode
// byte is set (whatever +0xB), Field_Request is 2 or Field_ScriptFlags bit
// 8, +3 = 1; DrawHud(0x5C, y).
void BoxSlideOut(const WorldMapTables& t) {
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
    AH_AT(DrawAt, t.fn_draw_hud)(0x5C, S16(o + 0x30));
}

// The dial frame at (x, y) (area 33's WorldMap_DrawFrame 0x404390,
// instruction for instruction), nothing unless Draw_PassFlags & 0x1B. A
// draw-mode primitive (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)) committed to
// slot 1; the dial (sprite 0) at (x, y); the cell under the leader
// (AreaMap_ByteAt of the high words of Field_Kind2X / Z); legend 1 at (x +
// 0x30, y) unless the cell is 0xA0 / 0xA1 / 0xAE or Field_ScriptFlags2 bit
// 12, its key (the first of six button entries whose mask has a bit of the
// button word 0) at (x + 0x38, y + 8) as the entry's sprite + 1 (lit) or the
// sprite (withheld); legend 2 at (x + 0x30, y + 0x10) unless the cell is 0xA0
// / 0xA1, its key from word 6 over EIGHT entries (the seventh and eighth are
// the record-8 state table's words) at (x + 0x38, y + 0x10); legend 3 at (x +
// 0x30, y + 0x18) when Field_CellHasEvent(cell x, z) or flag bit 12, or the
// party set is 0xC, or Field_ScriptFlags bit 14; the needle at (x + 0x18, y +
// 0x18).
void DrawFrame(const WorldMapTables& t, int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    const auto sprite = AH_AT(DrawSpriteAt, t.fn_draw_sprite);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x9C, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    sprite(x, y, 0);
    const unsigned char cell = AH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                                       static_cast<short>(Word(At(at::kLeaderCellZ))));
    const bool withheld1 = cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Field_ScriptFlags2 & 0x1000) != 0;
    if (!withheld1) sprite(x + 0x30, y, 1);
    {
        const int key = KeyIndex(t, static_cast<U>(Long(At(at::kButtonMap0))), 6);
        if (key >= 0) sprite(x + 0x38, y + 8, static_cast<unsigned>(withheld1 ? key : ((key + 1) & 0xFF)));
    }
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) sprite(x + 0x30, y + 0x10, 2);
    {
        const int key = KeyIndex(t, static_cast<U>(Long(At(at::kButtonMap6))), 8);
        if (key >= 0) sprite(x + 0x38, y + 0x10, static_cast<unsigned>(withheld2 ? key : ((key + 1) & 0xFF)));
    }
    bool third = AH_CALL(Field_CellHasEvent)(static_cast<short>(Word(At(at::kLeaderCellX))),
                                            static_cast<short>(Word(At(at::kLeaderCellZ)))) != 0;
    if (!third) third = (Field_ScriptFlags2 & 0x1000) != 0;
    if (!third) third = (At(at::kPartySet)[0] & 0x7F) == 0xC;
    if (!third) third = (Field_ScriptFlags & 0x4000) != 0;
    if (third) sprite(x + 0x30, y + 0x18, 3);
    AH_CALL(WorldMap_DrawNeedle)(x + 0x18, y + 0x18);
}

// One sprite of the dial page at (x, y) (area 33's WorldMap_DrawSprite
// 0x404560): a draw-mode primitive committed to slot 1, then at
// Gfx_PacketNext (read again) a SPRT, semi-transparent when the index's low
// byte is not 0, colour 0x80 x 3, x and y as floats of the arguments' low
// words, CLUT 0x7B80, (w, h, u, v) from the sprite table by index & 0xFF
// (unchecked); Gfx_CommitPrim(1, 0x1C).
void DrawSprite(const WorldMapTables& t, int x, int y, unsigned index) {
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
    const unsigned char* const entry = At(t.sprites + (index & 0xFF) * 4u);
    SetWord(prim + 0x18, entry[0]);
    SetWord(prim + 0x1A, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    AH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// The region box (area 33's WorldMap_DrawHud 0x404620): nothing unless
// Draw_PassFlags & 0x1B; the box (sprite 4) at (x, y), its cap (5) at (x +
// 0x80, y), then Text_DrawAt(x + 4, y + 4, 0, 0xFF, 0x803580 + the low word
// of the copy's label dword, read after the sprites - 0x803580 itself for
// areas 87 and 88, 0x803584 for area 45, 0x803588 for area 16).
void DrawHud(const WorldMapTables& t, int x, int y) {
    if ((Draw_PassFlags & 0x1B) == 0) return;
    AH_AT(DrawSpriteAt, t.fn_draw_sprite)(x, y, 4);
    AH_AT(DrawSpriteAt, t.fn_draw_sprite)(x + 0x80, y, 5);
    const unsigned char* const text = At(at::kAreaText + (static_cast<U>(Long(At(t.label_offset))) & 0xFFFF));
    AH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, text);
}

// The record's +8 state 1 (area 33's copy 0x4046A0):
// Sprite_SetAnimationBank(0x46); +0x48 = +0x24 = 0, +0x29 = 5; the direction
// d = +8 (unchecked): +0xC / +0x10 = the (s16) words of the direction table's
// entry d; +0x34 / +0x38 = the leader's position. Unless +6 is 0: the words
// +0x36 / +0x3A less each direction word >> 13 (arithmetic, 16-bit), then
// +0x36 (when +0xC is 0) or +0x3A moved by 2 - up for +6 == 1, down
// otherwise. Then the words +0x36 / +0x3A less the direction words >> 9;
// +0xB = 0, +1 + 1; Sprite_SetAnimation(the record-8 animation entry d's
// byte 0), +0x2A = its byte 1 (d read again after the call); a tail jump to
// Sprite_UpdateScreen.
void Record8Place(const WorldMapTables& t) {
    const auto dir = [&t](const unsigned char* o, U half) { return static_cast<std::int16_t>(Word(At(t.directions + half + o[8] * 4u))); };
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
    AH_CALL(Sprite_SetAnimation)(At(t.record8_anims + Cur()[8] * 2u)[0]);
    o = Cur();
    o[0x2A] = At(t.record8_anims + 1 + o[8] * 2u)[0];
    AH_CALL(Sprite_UpdateScreen)();
}

// The record's +4 state 0 (area 33's copy 0x404820): with the byte 0x903A79
// 9 or Field_StatusBits bit 0, a tail jump to Effect_Release. Else
// WorldMap_RecordIndex (its answer not read), Sprite_SetAnimationBank(0x205);
// +0x48, +0x24, +0x2A, +0x5D, +0x5E, +0x5F = 0; the map byte of the cell
// record +0xB (x, z; unchecked) set to 0xA0 - AreaMap_Bytes +
// AreaMap_Header[0] * z + x; Sprite_SetAnimation(0) and +1 = 1.
void Record4MarkCell(const WorldMapTables& t) {
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
    const U record = t.cells + Cur()[0xB] * 4u;
    const U z = At(record + 1)[0];
    const U x = At(record)[0];
    const_cast<unsigned char*>(AreaMap_Bytes)[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = 1;
}

// The drift layer (the record's +0x10, through WorldMap_RecordHook10; area
// 33's WorldMap33_DrawDrift 0x4048E0 instruction for instruction):
// docs/worldmap_area.md section 4. Once (+2 == 0): the word +0x3A +=
// Frame_Counter & 0xF, +2 + 1. Nothing more unless Draw_PassFlags bit 2. With
// b = +0xB and e = b - 2: +0x38 += b << 10; +0x3A = -8 above the map's height
// + 8; nothing more unless the leader is within 25 cells in x OR in z; one
// textured square through the GTE, then a grid of (17 - 4b) x (16 - 4b)
// semi-transparent map-item quads with u / v from the copy's drift table by e
// (unchecked).
void DrawDrift(const WorldMapTables& t) {
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
            const U m = At(t.drift_size + static_cast<U>(e))[0];
            const U ubase = At(t.drift_u + static_cast<U>(e))[0];
            const U vbase = At(t.drift_v + static_cast<U>(e))[0];
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

}  // namespace

// ===========================================================================
// Area 87: the world map's fifth copy (WorldMap_Records record 4)
// ===========================================================================

// original 0x40FC60 (WorldMap_FieldHooks 0x662DF0 entry 4; area 45's
// 0x407B40): PlaceMessage over area 87's tables - ten place rows 0x6124CC,
// cell records 0x611CC0, name sets of (id, four items) 0x61260C, four text
// rows.
extern "C" void __cdecl Area87_PlaceMessage(void) { PlaceMessage(at::kWm87); }
// original 0x40FDE0 (WorldMap_Records[4] +0; area 45's 0x407CC0): PlateRun
// through Area87_PlateStates 0x61261C.
extern "C" void __cdecl Area87_PlateRun(void) { PlateRun(at::kWm87); }
// original 0x40FEC0 (Area87_PlateStates 0; area 45's 0x407DA0): PlateStart
// with bank 0x156 (pushed as an imm32: the function is 3 bytes longer than
// area 45's).
extern "C" void __cdecl Area87_PlateStart(void) { PlateStart(at::kWm87); }
// original 0x40FF20 (Area87_PlateStates 1; area 45's 0x407DF0): PlateShow
// over Area87_PlateAnims 0x611C98 (ten entries).
extern "C" void __cdecl Area87_PlateShow(void) { PlateShow(at::kWm87); }
// original 0x410070 (Area87_PlateStates 2; area 45's 0x407F40): PlateGrow.
extern "C" void __cdecl Area87_PlateGrow(void) { PlateGrow(); }
// original 0x4100C0 (Area87_PlateStates 3; area 45's 0x407F90): PlateHold.
extern "C" void __cdecl Area87_PlateHold(void) { PlateHold(); }
// original 0x410120 (Area87_PlateStates 4; area 45's 0x407FF0): PlateShrink.
extern "C" void __cdecl Area87_PlateShrink(void) { PlateShrink(); }
// original 0x410170 (record 4 +0xC; area 45's 0x408040): Area87_HudStates
// 0x612630 by +1 - WorldMapHud_Start (shared), Area87_HudFrame.
extern "C" void __cdecl Area87_HudRun(void) { StateEntry("Area87", "HudRun", at::kWm87.hud_states, 2, Cur()[1])(); }
// original 0x410190 (Area87_HudStates 1): `call 0x4101A0; jmp 0x410270` - the
// frame's slide, then the region box's.
extern "C" void __cdecl Area87_HudFrame(void) {
    area_harness::Phase(at::kWm87.fn_frame_step)();
    area_harness::Phase(at::kWm87.fn_box_step)();
}
// original 0x4101A0 (called by Area87_HudFrame): Area87_FrameStates 0x612638
// by +2 - WorldMap_FrameWait (shared), _FrameSlideIn, _FrameHold,
// _FrameSlideOut. A tail jump; ours aborts past the four entries.
extern "C" void __cdecl Area87_FrameStep(void) { StateEntry("Area87", "FrameStep", at::kWm87.frame_states, 4, Cur()[2])(); }
// original 0x4101C0 (Area87_FrameStates 1): FrameSlideIn, then a tail jump to
// Area87_FrameHold 0x4101F0.
extern "C" void __cdecl Area87_FrameSlideIn(void) { FrameSlideIn(at::kWm87); }
// original 0x4101F0 (Area87_FrameStates 2): FrameHold, drawing through
// Area87_DrawFrame 0x4103D0.
extern "C" void __cdecl Area87_FrameHold(void) { FrameHold(at::kWm87); }
// original 0x410220 (Area87_FrameStates 3): FrameSlideOut.
extern "C" void __cdecl Area87_FrameSlideOut(void) { FrameSlideOut(at::kWm87); }
// original 0x410270 (called by Area87_HudFrame): Area87_BoxStates 0x612648
// by +3 - WorldMapHud_BoxWait (shared), _BoxSlideIn, _BoxHold, _BoxSlideOut.
extern "C" void __cdecl Area87_BoxStep(void) { StateEntry("Area87", "BoxStep", at::kWm87.box_states, 4, Cur()[3])(); }
// original 0x410290 (Area87_BoxStates 1): BoxSlideIn, drawing through
// Area87_DrawHud 0x410660.
extern "C" void __cdecl Area87_BoxSlideIn(void) { BoxSlideIn(at::kWm87); }
// original 0x410300 (Area87_BoxStates 2): BoxHold.
extern "C" void __cdecl Area87_BoxHold(void) { BoxHold(at::kWm87); }
// original 0x410370 (Area87_BoxStates 3): BoxSlideOut.
extern "C" void __cdecl Area87_BoxSlideOut(void) { BoxSlideOut(at::kWm87); }
// original 0x4103D0 (called by the frame states): DrawFrame over
// Area87_Buttons 0x6126B0, drawing through Area87_DrawSprite 0x4105A0.
extern "C" void __cdecl Area87_DrawFrame(int x, int y) { DrawFrame(at::kWm87, x, y); }
// original 0x4105A0: DrawSprite over Area87_Sprites 0x612658.
extern "C" void __cdecl Area87_DrawSprite(int x, int y, unsigned index) { DrawSprite(at::kWm87, x, y, index); }
// original 0x410660 (called by the box states): DrawHud, the label's offset
// the low word of the dword 0x803580.
extern "C" void __cdecl Area87_DrawHud(int x, int y) { DrawHud(at::kWm87, x, y); }
// original 0x4106C0 (record 4 +8): Area87_Record8States 0x6126C8 by +1 -
// 0x4253C0 and 0x40C490 (the copies' shared states, not this group's)
// around Area87_Record8Place.
extern "C" void __cdecl Area87_Record8Run(void) { StateEntry("Area87", "Record8Run", at::kWm87.record8_states, 3, Cur()[1])(); }
// original 0x4106E0 (Area87_Record8States 1): Record8Place over
// Area87_Directions 0x6126D4 and Area87_Record8Anims 0x6126E4.
extern "C" void __cdecl Area87_Record8Place(void) { Record8Place(at::kWm87); }
// original 0x410840 (record 4 +4): Area87_Record4States 0x6126EC by +1 -
// Area87_Record4MarkCell, then Area45_Record4Tick 0x408990 (shared).
extern "C" void __cdecl Area87_Record4Run(void) { StateEntry("Area87", "Record4Run", at::kWm87.record4_states, 2, Cur()[1])(); }
// original 0x410860 (Area87_Record4States 0): Record4MarkCell over the cell
// records 0x611CC0.
extern "C" void __cdecl Area87_Record4MarkCell(void) { Record4MarkCell(at::kWm87); }
// original 0x410920 (record 4 +0x10): DrawDrift over Area87_DriftUV 0x6126F4.
extern "C" void __cdecl Area87_DrawDrift(void) { DrawDrift(at::kWm87); }

// ===========================================================================
// Area 88: the world map's sixth copy (WorldMap_Records record 5; the
// world-map route plays it)
// ===========================================================================

// original 0x410D90 (WorldMap_FieldHooks entry 5; area 45's 0x407B40 but for
// the name sets): PlaceMessage over area 88's tables - seven place rows
// 0x612E94, cell records 0x61271C, name sets of (id, eleven items) 0x612F74
// (a stride of 12, the lea ecx, [ebp + ebp*2] and lea edi, [ecx*4 + ..] the
// four bytes longer than area 45's), eleven text rows (to 0x904E40).
extern "C" void __cdecl Area88_PlaceMessage(void) { PlaceMessage(at::kWm88); }
// original 0x410F10 (WorldMap_Records[5] +0): PlateRun through
// Area88_PlateStates 0x612F98.
extern "C" void __cdecl Area88_PlateRun(void) { PlateRun(at::kWm88); }
// original 0x410FF0 (Area88_PlateStates 0): PlateStart with bank 0x157.
extern "C" void __cdecl Area88_PlateStart(void) { PlateStart(at::kWm88); }
// original 0x411050 (Area88_PlateStates 1): PlateShow over Area88_PlateAnims
// 0x612700 (seven entries).
extern "C" void __cdecl Area88_PlateShow(void) { PlateShow(at::kWm88); }
// original 0x4111A0 (Area88_PlateStates 2): PlateGrow.
extern "C" void __cdecl Area88_PlateGrow(void) { PlateGrow(); }
// original 0x4111F0 (Area88_PlateStates 3): PlateHold.
extern "C" void __cdecl Area88_PlateHold(void) { PlateHold(); }
// original 0x411250 (Area88_PlateStates 4): PlateShrink.
extern "C" void __cdecl Area88_PlateShrink(void) { PlateShrink(); }
// original 0x4112C0 (record 5 +0xC): Area88_HudStates 0x612FAC by +1.
extern "C" void __cdecl Area88_HudRun(void) { StateEntry("Area88", "HudRun", at::kWm88.hud_states, 2, Cur()[1])(); }
// original 0x4112E0 (Area88_HudStates 1): `call 0x4112F0; jmp 0x4113F0`.
extern "C" void __cdecl Area88_HudFrame(void) {
    area_harness::Phase(at::kWm88.fn_frame_step)();
    area_harness::Phase(at::kWm88.fn_box_step)();
}
// original 0x4112F0: Area88_FrameStates 0x612FB4 by +2 (entry 0
// WorldMap_FrameWait 0x411310, the next address).
extern "C" void __cdecl Area88_FrameStep(void) { StateEntry("Area88", "FrameStep", at::kWm88.frame_states, 4, Cur()[2])(); }
// original 0x411340 (Area88_FrameStates 1): FrameSlideIn, then Area88_FrameHold.
extern "C" void __cdecl Area88_FrameSlideIn(void) { FrameSlideIn(at::kWm88); }
// original 0x411370 (Area88_FrameStates 2): FrameHold through Area88_DrawFrame.
extern "C" void __cdecl Area88_FrameHold(void) { FrameHold(at::kWm88); }
// original 0x4113A0 (Area88_FrameStates 3): FrameSlideOut.
extern "C" void __cdecl Area88_FrameSlideOut(void) { FrameSlideOut(at::kWm88); }
// original 0x4113F0: Area88_BoxStates 0x612FC4 by +3.
extern "C" void __cdecl Area88_BoxStep(void) { StateEntry("Area88", "BoxStep", at::kWm88.box_states, 4, Cur()[3])(); }
// original 0x411410 (Area88_BoxStates 1): BoxSlideIn through Area88_DrawHud.
extern "C" void __cdecl Area88_BoxSlideIn(void) { BoxSlideIn(at::kWm88); }
// original 0x411480 (Area88_BoxStates 2): BoxHold.
extern "C" void __cdecl Area88_BoxHold(void) { BoxHold(at::kWm88); }
// original 0x4114F0 (Area88_BoxStates 3): BoxSlideOut.
extern "C" void __cdecl Area88_BoxSlideOut(void) { BoxSlideOut(at::kWm88); }
// original 0x411550: DrawFrame over Area88_Buttons 0x61302C.
extern "C" void __cdecl Area88_DrawFrame(int x, int y) { DrawFrame(at::kWm88, x, y); }
// original 0x411720: DrawSprite over Area88_Sprites 0x612FD4.
extern "C" void __cdecl Area88_DrawSprite(int x, int y, unsigned index) { DrawSprite(at::kWm88, x, y, index); }
// original 0x4117E0: DrawHud, the label's offset the low word of 0x803580.
extern "C" void __cdecl Area88_DrawHud(int x, int y) { DrawHud(at::kWm88, x, y); }
// original 0x411840 (record 5 +8): Area88_Record8States 0x613044 by +1.
extern "C" void __cdecl Area88_Record8Run(void) { StateEntry("Area88", "Record8Run", at::kWm88.record8_states, 3, Cur()[1])(); }
// original 0x411860 (Area88_Record8States 1): Record8Place over
// Area88_Directions 0x613050 and Area88_Record8Anims 0x613060.
extern "C" void __cdecl Area88_Record8Place(void) { Record8Place(at::kWm88); }
// original 0x4119C0 (record 5 +4): Area88_Record4States 0x613068 by +1.
extern "C" void __cdecl Area88_Record4Run(void) { StateEntry("Area88", "Record4Run", at::kWm88.record4_states, 2, Cur()[1])(); }
// original 0x4119E0 (Area88_Record4States 0): Record4MarkCell over 0x61271C.
extern "C" void __cdecl Area88_Record4MarkCell(void) { Record4MarkCell(at::kWm88); }
// original 0x411AA0 (record 5 +0x10): DrawDrift over Area88_DriftUV 0x613070.
extern "C" void __cdecl Area88_DrawDrift(void) { DrawDrift(at::kWm88); }

// ===========================================================================

void AreaW2b_Inject() {
    if (bof3::WantsShadow("area_w2b")) area_w2b::SelfTest();
    BOF3_INJECT(Area85_Kind2Walk);
    BOF3_INJECT(Area85_FollowMember2);
    BOF3_INJECT(Area85_SetType7);
    BOF3_INJECT(Area85_SpawnEffect47A);
    BOF3_INJECT(Area85_SpawnEffect47B);
    BOF3_INJECT(Area85_SetUpObject);
    BOF3_INJECT(Area85_StartSlotScript);
    BOF3_INJECT(Area85_ReleaseSlots);
    BOF3_INJECT(Area85_Raise18);
    BOF3_INJECT(Area85_Sound201);
    BOF3_INJECT(Area85_Init);
    BOF3_INJECT(Area85_ClutShift);
    BOF3_INJECT(Area86_SwitchHook);
    BOF3_INJECT(Area86_MemberNear);
    BOF3_INJECT(Area86_Init);
    BOF3_INJECT(Area87_Init);
    BOF3_INJECT(Area87_PlaceMessage);
    BOF3_INJECT(Area87_PlateRun);
    BOF3_INJECT(Area87_PlateStart);
    BOF3_INJECT(Area87_PlateShow);
    BOF3_INJECT(Area87_PlateGrow);
    BOF3_INJECT(Area87_PlateHold);
    BOF3_INJECT(Area87_PlateShrink);
    BOF3_INJECT(Area87_HudRun);
    BOF3_INJECT(Area87_HudFrame);
    BOF3_INJECT(Area87_FrameStep);
    BOF3_INJECT(Area87_FrameSlideIn);
    BOF3_INJECT(Area87_FrameHold);
    BOF3_INJECT(Area87_FrameSlideOut);
    BOF3_INJECT(Area87_BoxStep);
    BOF3_INJECT(Area87_BoxSlideIn);
    BOF3_INJECT(Area87_BoxHold);
    BOF3_INJECT(Area87_BoxSlideOut);
    BOF3_INJECT(Area87_DrawFrame);
    BOF3_INJECT(Area87_DrawSprite);
    BOF3_INJECT(Area87_DrawHud);
    BOF3_INJECT(Area87_Record8Run);
    BOF3_INJECT(Area87_Record8Place);
    BOF3_INJECT(Area87_Record4Run);
    BOF3_INJECT(Area87_Record4MarkCell);
    BOF3_INJECT(Area87_DrawDrift);
    BOF3_INJECT(Area88_PlaceMessage);
    BOF3_INJECT(Area88_PlateRun);
    BOF3_INJECT(Area88_PlateStart);
    BOF3_INJECT(Area88_PlateShow);
    BOF3_INJECT(Area88_PlateGrow);
    BOF3_INJECT(Area88_PlateHold);
    BOF3_INJECT(Area88_PlateShrink);
    BOF3_INJECT(Area88_HudRun);
    BOF3_INJECT(Area88_HudFrame);
    BOF3_INJECT(Area88_FrameStep);
    BOF3_INJECT(Area88_FrameSlideIn);
    BOF3_INJECT(Area88_FrameHold);
    BOF3_INJECT(Area88_FrameSlideOut);
    BOF3_INJECT(Area88_BoxStep);
    BOF3_INJECT(Area88_BoxSlideIn);
    BOF3_INJECT(Area88_BoxHold);
    BOF3_INJECT(Area88_BoxSlideOut);
    BOF3_INJECT(Area88_DrawFrame);
    BOF3_INJECT(Area88_DrawSprite);
    BOF3_INJECT(Area88_DrawHud);
    BOF3_INJECT(Area88_Record8Run);
    BOF3_INJECT(Area88_Record8Place);
    BOF3_INJECT(Area88_Record4Run);
    BOF3_INJECT(Area88_Record4MarkCell);
    BOF3_INJECT(Area88_DrawDrift);
}
