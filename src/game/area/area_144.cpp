// Moved here by tools/move_funcs.py from area_w3f.cpp (docs/source-layout-round.md).
#include "game/area/area_144.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

U Key(const void* p) { return Low32(p); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// Area 143's handler 3 (0x4209F0, also area 197's handler 9) and area 144's
// handlers 2 and 3 (0x421000, 0x421020): a byte equal to `value`, the script
// position + 3.
void SkipIf(U cell, unsigned char value) {
    if (B(cell) == value) ScriptStep(3);
}

// (target - the running object's z) >> 15, 32-bit, arithmetic.
std::int32_t StepsToZ(U target) { return static_cast<std::int32_t>(target - static_cast<U>(rv::Current<records::SpriteObject>()->z)) >> 15; }

unsigned char Magnitude(std::int32_t d) { return static_cast<unsigned char>(d < 0 ? 0u - static_cast<U>(d) : static_cast<U>(d)); }

}  // namespace

// original 0x420DD0 (area 144 +0x3C[0]; PSX 0x801F3818): by the chapter
// row's flags 0x17..0x1B (the first clear) and the leader's +0x89, the script
// object's +3 = a scene (2..0xD), a flag of the row set on three of them;
// the script position = 0xFFFE (stored, not added) every way out.
extern "C" void __cdecl Area144_SceneByRowFlags(void) {
    auto scene = [](unsigned char n) { MoveScript_Object[3] = n; };
    auto rewind = [] { rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, 0xFFFE); };
    auto set_row = [](unsigned index) { AH_CALL(Flags_Set)(FlagRow(), index); };
    if (AH_CALL(Flags_Test)(FlagRow(), 0x17) == 0) {
        scene(2);
        rewind();
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x18) == 0) {
        if (B(::bof3::Img(ObjTrio_addr) + 0x89) == 2) {
            scene(4);
            set_row(0x18);
        } else {
            scene(3);
        }
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x19) == 0) {
        const unsigned char who = B(::bof3::Img(ObjTrio_addr) + 0x89);
        if (who == 8) {
            scene(7);
            set_row(0x19);
        } else {
            scene(who == 5 ? 6 : 5);
        }
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x1A) == 0) {
        const unsigned char who = B(::bof3::Img(ObjTrio_addr) + 0x89);
        scene(who == 7 ? 0xA : who == 8 ? 9 : 8);
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x1B) == 0) {
        if (B(::bof3::Img(ObjTrio_addr) + 0x89) == 7) {
            scene(0xD);
            set_row(0x1B);
        } else {
            scene(0xC);
        }
    }
    rewind();
}

// original 0x420FA0 (area 144 +0x3C[1]; PSX 0x801F3A20): d = (0x1D8000 -
// the running object's z) >> 15; not 0: the script object's +7 = |d|, the
// running object's direction 1 (d negative) or 5, MoveCmd_Move(the script
// object, that direction).
extern "C" void __cdecl Area144_WalkToZ1D8(void) {
    const std::int32_t d = StepsToZ(0x1D8000);
    if (d == 0) return;
    MoveScript_Object[7] = Magnitude(d);
    Sprite_Current[8] = d < 0 ? 1 : 5;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
}

// original 0x421000 (area 144 +0x3C[2]; PSX 0x801F3AB4): the third party
// record's +0x89 4, the script position + 3.
extern "C" void __cdecl Area144_SkipIfMember3Is4(void) { SkipIf(::bof3::Img(ObjTrio_addr) + 0x321, 4); }

// original 0x421020 (area 144 +0x3C[3]; PSX 0x801F3AEC): the third record's
// +0x89 2, the script position + 3.
extern "C" void __cdecl Area144_SkipIfMember3Is2(void) { SkipIf(::bof3::Img(ObjTrio_addr) + 0x321, 2); }

// original 0x421040 (area 144 +0x3C[4]; PSX 0x801F3B24): d = (0x1C0000 -
// the running object's z) >> 15; not 0: the running object's direction 1,
// the script object's +7 = |d|, MoveCmd_Move(the script object, direction).
// Field_ScriptFlags' low byte | 8 either way.
extern "C" void __cdecl Area144_WalkToZ1C0(void) {
    unsigned char* const cur = Sprite_Current;
    const std::int32_t d = StepsToZ(0x1C0000);
    if (d != 0) {
        cur[8] = 1;
        MoveScript_Object[7] = Magnitude(d);
        AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
    }
    B(::bof3::Img(Field_ScriptFlags_addr)) = static_cast<unsigned char>(B(::bof3::Img(Field_ScriptFlags_addr)) | 8);
}

// original 0x421090 (area 144 +0x3C[5]; PSX 0x801F3BAC): Effect_FindFree;
// none: the script position - 2. A slot: kind 0x85, +6 the active member's
// index in Sprite_Objects ((pointer - 0x7DEE80) / 0xA4, signed, truncated).
extern "C" void __cdecl Area144_SpawnEffect85(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) {
        ScriptStep(-2);
        return;
    }
    const auto index = static_cast<std::int32_t>(Key(Field_ActiveMember) - ::bof3::Img(0x7DEE80u)) / 0xA4;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x85;
    e[6] = static_cast<unsigned char>(index);
}

// original 0x4210F0 (area 144 +0x3C[6]; PSX 0x801F3C78): the kind-2 object
// (Sprite_Kind2) placed at the running object's (x, z, y), its +0x84 the
// script object's +4, its +0x87 1; MoveCmd_MoveKind2(3).
extern "C" void __cdecl Area144_MoveKind2Here(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    unsigned char* const k2 = Mem(::bof3::Img(Sprite_Kind2_addr));
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(k2)->x, tCur->x);
    const std::int32_t z = tCur->z;
    const unsigned char* const so = MoveScript_Object;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(k2)->z, z);
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(k2)->height, tCur->height);
    k2[0x84] = so[4];
    k2[0x87] = 1;
    AH_CALL(MoveCmd_MoveKind2)(3);
}

// original 0x421130 (area 144 +0x34[0] = +0x3C[7]; PSX 0x801F3CE4): message
// 0xFFFF; counter 0 = 0x23 for an answer, else 0x1E.
extern "C" void __cdecl Area144_ChoiceCounter1E(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(::bof3::Img(at::kCounter0.v)) = answer != 0 ? 0x23 : 0x1E;
}

// original 0x421150 (area 144 +0x34[1] = +0x3C[8]; PSX 0x801F3D14): message
// 0xFFFF; answer 0: counter 0 = 5, Party_DropIn(6), MoveScript_Var7 7, the
// step 0x14; else Party_DropIn(5).
extern "C" void __cdecl Area144_ChoiceDropIn(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) {
        AH_CALL(Party_DropIn)(5);
        return;
    }
    B(::bof3::Img(at::kCounter0.v)) = 5;
    AH_CALL(Party_DropIn)(6);
    MoveScript_Var7 = 7;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x14;
}

void Area144_Inject() {
    BOF3_INJECT(Area144_SceneByRowFlags);
    BOF3_INJECT(Area144_WalkToZ1D8);
    BOF3_INJECT(Area144_SkipIfMember3Is4);
    BOF3_INJECT(Area144_SkipIfMember3Is2);
    BOF3_INJECT(Area144_WalkToZ1C0);
    BOF3_INJECT(Area144_SpawnEffect85);
    BOF3_INJECT(Area144_MoveKind2Here);
    BOF3_INJECT(Area144_ChoiceCounter1E);
    BOF3_INJECT(Area144_ChoiceDropIn);
}
