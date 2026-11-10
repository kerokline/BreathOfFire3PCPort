// Moved here by tools/move_funcs.py from area_w1b.cpp (docs/source-layout-round.md).
#include "game/area/area_43.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using area_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* Cur() { return Sprite_Current; }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

unsigned char* ActiveMember() { return Field_ActiveMember; }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4u)))));
}

}  // namespace

// original 0x406F80 (area 43's handler 0, Area43_Handlers 0x5F6A18): a tail
// jump through Area43_States 0x5F6A6C by Sprite_Current[4] (ours aborts past
// its two entries).
extern "C" void __cdecl Area43_ObjectRun(void) { StateEntry("Area43_ObjectRun", ::bof3::Img(Area43_States_addr), 2, Cur()[4])(); }

// original 0x406FA0 (Area43_States 0): Field_ActiveMember +0x9F =
// Sprite_SetTint(Sprite_Current, 0, 0, 0, 1) (the member read after the
// call); Sprite_Current +0 bit 5; the tint bytes +0x5F, +0x5E, +0x5D = 0xC0;
// +0x5C = 1, +0x48 = 2, +4 = 1 (state 1); Field_ActiveMember's word +0x8A - 2
// (the object's script position held back two bytes: the op runs again).
// Sprite_Current is read again for each store.
extern "C" void __cdecl Area43_TintStart(void) {
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Cur(), 0, 0, 0, 1);
    ActiveMember()[0x9F] = tint;
    Cur()[0] = static_cast<unsigned char>(Cur()[0] | 0x20);
    Cur()[0x5F] = 0xC0;
    Cur()[0x5E] = 0xC0;
    Cur()[0x5D] = 0xC0;
    Cur()[0x5C] = 1;
    Cur()[0x48] = 2;
    Cur()[4] = 1;
    unsigned char* const m = ActiveMember();
    rv::Store<unsigned>(rv::As<records::SpriteObject>(m)->script_position, rv::As<records::SpriteObject>(m)->script_position + 0xFFFEu);
}

// original 0x407020 (Area43_States 1): each tint byte +0x5D, +0x5E, +0x5F not
// yet 0x80 less 2; the dwords +0x40 and +0x44 + 0x800. When all three are
// 0x80: Tint_Release(Field_ActiveMember +0x9F), +0 bit 6, +4 = 0. Else
// Field_ActiveMember's word +0x8A - 2.
extern "C" void __cdecl Area43_TintSettle(void) {
    for (const U off : {0x5Du, 0x5Eu, 0x5Fu}) {
        unsigned char* const o = Cur();
        if (o[off] != 0x80) o[off] = static_cast<unsigned char>(o[off] - 2);
    }
    AddLong(Cur() + 0x40, 0x800);
    AddLong(Cur() + 0x44, 0x800);
    const unsigned char* const o = Cur();
    if (o[0x5D] == 0x80 && o[0x5E] == 0x80 && o[0x5F] == 0x80) {
        AH_CALL(Tint_Release)(ActiveMember()[0x9F]);
        Cur()[0] = static_cast<unsigned char>(Cur()[0] | 0x40);
        Cur()[4] = 0;
        return;
    }
    unsigned char* const m = ActiveMember();
    rv::Store<unsigned>(rv::As<records::SpriteObject>(m)->script_position, rv::As<records::SpriteObject>(m)->script_position + 0xFFFEu);
}

// original 0x4070D0 (area 43's handler 1): Effect_FindFree to the byte
// 0x903850; a slot: its record (the slot re-read) +0 = 1, kind +5 = 0x37,
// the word +0x2C = 0x64, +9 = 1, +6 = 0, +0x34 / +0x38 = Sprite_Current's,
// +0x3C = Sprite_Current's + 0x800000.
extern "C" void __cdecl Area43_SpawnEffect37(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    at::kFoundSlot = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(at::kFoundSlot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x37;
    rv::Store<unsigned>(rv::As<records::EffectObject>(e)->column, 0x64);
    e[9] = 1;
    const unsigned char* const o = Cur();
    const auto* const tO = rv::AsConst<records::SpriteObject>(o);
    e[6] = 0;
    tE->x = tO->x;
    tE->z = tO->z;
    tE->height = static_cast<std::int32_t>(static_cast<U>(tO->height) + 0x800000u);
}

// original 0x407140 (area 43's handler 2): Sprite_ReleaseTint(Sprite_Current),
// Sprite_SetAnimationBank(0x1BD), Sprite_SetAnimation(0); +0x2A = 0, +0x40 =
// +0x44 = 0xE000; Field_ActiveMember +0x9F = Sprite_SetTint(Sprite_Current,
// 0, 0, 0, 1); +0x5C = 1, the tint bytes +0x5F, +0x5E, +0x5D = 0x14, +0x2B = 3.
extern "C" void __cdecl Area43_SetUpObject(void) {
    AH_CALL(Sprite_ReleaseTint)(Cur());
    AH_CALL(Sprite_SetAnimationBank)(0x1BD);
    AH_CALL(Sprite_SetAnimation)(0);
    Cur()[0x2A] = 0;
    rv::Current<records::SpriteObject>()->scale_x = 0xE000;
    rv::Current<records::SpriteObject>()->scale_y = 0xE000;
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Cur(), 0, 0, 0, 1);
    ActiveMember()[0x9F] = tint;
    Cur()[0x5C] = 1;
    Cur()[0x5F] = 0x14;
    Cur()[0x5E] = 0x14;
    Cur()[0x5D] = 0x14;
    Cur()[0x2B] = 3;
}

// original 0x4071E0 (Field_ObjectTriggers id 58, called (object, 0x904030)
// and reading neither; a gap function by the tool, placed in area 43's block
// by address): story flag 0x7F set, MoveCmd_TestFB(0x44, 0xB) (its answer not
// read), Sound_PlayEffect(0x103).
extern "C" void __cdecl Area43_Trigger58(void) {
    AH_CALL(Flags_Set)(Bank(), 0x7F);
    AH_CALL(MoveCmd_TestFB)(0x44, 0xB);
    AH_CALL(Sound_PlayEffect)(0x103);
}

void Area43_Inject() {
    BOF3_INJECT(Area43_ObjectRun);
    BOF3_INJECT(Area43_TintStart);
    BOF3_INJECT(Area43_TintSettle);
    BOF3_INJECT(Area43_SpawnEffect37);
    BOF3_INJECT(Area43_SetUpObject);
    BOF3_INJECT(Area43_Trigger58);
}
