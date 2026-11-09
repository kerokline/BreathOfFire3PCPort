// Moved here by tools/move_funcs.py from area_w2d.cpp (docs/source-layout-round.md).
#include "game/area/area_95.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// Field_ActiveMember's word +0x8A less 2.
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }

// Areas 95 and 96's handler 4 (0x4135B0, 0x413860, byte for byte the same):
// the running object at z word 0x3A and x word 0x40 turns to 3, at x 0x42 to
// 7, and runs Area95_TurnAndMove (the originals' tail jmp); anywhere else
// the script position + 2.
void TurnAtCell() {
    unsigned char* const cur = Sprite_Current;
    if (rv::As<records::SpriteObject>(cur)->z_whole == 0x3A) {
        const auto d = static_cast<std::uint16_t>(rv::As<records::SpriteObject>(cur)->x_whole - 0x40);
        if (d == 0 || d == 2) {
            cur[8] = d == 0 ? 3 : 7;
            AH_CALL(Area95_TurnAndMove)();
            return;
        }
    }
    ScriptStep(2);
}

}  // namespace

// original 0x4135B0 (area 95 +0x3C[4]; PSX 0x801F351C): TurnAtCell.
extern "C" void __cdecl Area95_TurnAtCell(void) { TurnAtCell(); }

// original 0x4135F0 (areas 95 and 96 +0x3C[0]; PSX 0x801F30E8 / 0x801F3078):
// the first of the twenty effect records that is live (+0 bit 0) and stands
// at x word 0x41, z word 0x3A: while there is one, the script position - 2.
// None: with the leader's byte +0x137 set, the script position - 2 and
// ScriptFlags_Set40; clear, Party_DropIn(0).
extern "C" void __cdecl Area95_WaitEffectAtCell(void) {
    unsigned i = 0;
    for (; i < at::kEffectCount; ++i) {
        const unsigned char* const e = EffectAt(i);
        if ((e[0] & 1) != 0 && rv::As<records::EffectObject>(e)->x_whole == 0x41 && rv::As<records::EffectObject>(e)->z_whole == 0x3A) break;
    }
    if (i != at::kEffectCount) {
        ScriptStep(-2);
        return;
    }
    if (B(::bof3::Img(ObjTrio_addr) + 0x137) != 0) {
        ScriptStep(-2);
        AH_CALL(ScriptFlags_Set40)();
        return;
    }
    AH_CALL(Party_DropIn)(0);
}

// original 0x413670 (areas 95 and 96 +0x3C[1]; PSX 0x801F31F8 / 0x801F3188):
// Effect_FindFree; none: the active member's word +0x8A - 2. A slot: kind
// 0x37 at (0x410000, 0x3C8000, 0x2000000), +0x2C 0x58, +9 1, +6 0, +7 0x12,
// +0x29 6; Sound_PlayEffect(0x202); then story flag 0x20 in area 0x60 (96),
// else 0x21 below chapter 7 (Cond_ByteFA signed), else 0x22.
extern "C" void __cdecl Area95_SpawnEffect37(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) {
        MemberStep();
        return;
    }
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x37;
    tE->x = 0x410000;
    tE->z = 0x3C8000;
    tE->height = 0x2000000;
    rv::Store<unsigned>(rv::As<records::EffectObject>(e)->column, 0x58);
    e[9] = 1;
    e[6] = 0;
    e[7] = 0x12;
    e[0x29] = 6;
    AH_CALL(Sound_PlayEffect)(0x202);
    if (Game_AreaNumber == 0x60) {
        AH_CALL(Flags_Set)(StoryFlags(), 0x20);
        return;
    }
    AH_CALL(Flags_Set)(StoryFlags(), Cond_ByteFA < 7 ? 0x21u : 0x22u);
}

// original 0x413750 (areas 95 and 96 +0x3C[2]; PSX 0x801F336C / 0x801F32FC):
// the script object's +7 = 1 and MoveCmd_Move(it, 5); then, n the running
// object's +9, the ground at (x + +0xC * n, z + +0x10 * n) (dwords, wrapping)
// and +0x14 = (ground - the s16 +0x3E + 0xE0) / n (idiv: n 0 faults, ours
// aborts); MoveScript_PartyRecords record 0's word +4 = 0x40 and its count +1
// = n. Sprite_Current read again after each call.
extern "C" void __cdecl Area95_LeapArc(void) {
    MoveScript_Object[7] = 1;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 5);
    const unsigned char* cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    const U n = cur[9];
    const U x = static_cast<U>(tCur->step_x) * n + static_cast<U>(tCur->x);
    const U z = static_cast<U>(tCur->step_z) * n + static_cast<U>(tCur->z);
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z)));
    unsigned char* const obj = Sprite_Current;
    auto* const tObj = rv::As<records::SpriteObject>(obj);
    const std::int32_t rise = static_cast<std::int32_t>(ground) - static_cast<std::int16_t>(Word(obj + 0x3E)) + 0xE0;
    const unsigned frames = obj[9];
    if (frames == 0) bof3::Fatal("Area95_LeapArc: +9 is 0, the original's idiv faults");
    tObj->step_y = rise / static_cast<std::int32_t>(frames);
    SetLong(Mem(::bof3::Img(MoveScript_PartyRecords_addr) + 4), 0x40);
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 1) = Sprite_Current[9];
}

// original 0x4137D0 (areas 95 and 96 +0x3C[3]; PSX 0x801F3450 / 0x801F33E0;
// also the tail of both handler 4 copies): the script object's +4 = 1 and
// bit 0x40; the running object's +7 bit 8; Sprite_SetAnimation(8 + its
// direction & 7); the script object's +7 = 2; then MoveCmd_Move(the script
// object, 3) and direction 3 when it faced 7, else 7 and 7.
extern "C" void __cdecl Area95_TurnAndMove(void) {
    MoveScript_Object[4] = 1;
    const auto dir = static_cast<unsigned char>(Sprite_Current[8] & 7);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Sprite_Current[7] = static_cast<unsigned char>(Sprite_Current[7] | 8);
    AH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(dir + 8));
    MoveScript_Object[7] = 2;
    const unsigned char to = dir == 7 ? 3 : 7;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, to);
    Sprite_Current[8] = to;
}

void Area95_Inject() {
    BOF3_INJECT(Area95_TurnAtCell);
    BOF3_INJECT(Area95_WaitEffectAtCell);
    BOF3_INJECT(Area95_SpawnEffect37);
    BOF3_INJECT(Area95_LeapArc);
    BOF3_INJECT(Area95_TurnAndMove);
}
