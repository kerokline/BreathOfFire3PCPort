// Moved here by tools/move_funcs.py from area_w0c.cpp (docs/source-layout-round.md).
#include "game/area/area_32.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void AddLong(unsigned char* p, std::int32_t v) { SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) + static_cast<std::uint32_t>(v))); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Effect_FindFree's slot to the running object's +0xB, then (0xFF: none) a
// new effect record of `kind` at that slot: +0 = 1 and +5 = the kind, the
// slot re-read from the object for each store as the originals re-read it.
bool SpawnFromCurrent(unsigned char kind) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return false;
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = kind;
    return true;
}

// Area 32's two effects of kind 0x50 (0x403AF0, 0x403C40): the running
// object's +8 to the effect's +6, its position to the effect's +0x34..+0x3F
// raised by `lift` in +0x38 and by 0x100 in +0x3C's high word, and `timer`
// to +9.
void SpawnEffect50(std::int32_t lift, unsigned char timer) {
    if (!SpawnFromCurrent(0x50)) return;
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    EffectAt(cur[0xB])[6] = cur[8];
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->x, tCur->x);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->z) + static_cast<std::uint32_t>(lift)));
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->height) + 0x1000000u));
    EffectAt(cur[0xB])[9] = timer;
}

}  // namespace

// original 0x403880 (Area32_Choices[0]; PSX 0x801F363C): message 0xFFFF; answer 1 plays
// animation 0x80 on the leader (Sprite_Current made ObjTrio for the call,
// then put back).
extern "C" void __cdecl Area32_ChoiceLeaderAnim(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 1) return;
    unsigned char* const kept = Sprite_Current;
    Sprite_Current = rv::Bytes(ObjTrio);
    AH_CALL(Sprite_SetAnimation)(0x80);
    Sprite_Current = kept;
}

// original 0x4038C0 (Area32_Choices[2] = Area32_Handlers[0]; PSX
// 0x801F36E4): Area32_StatesA by Sprite_Current[4].
extern "C" void __cdecl Area32_RunA(void) {
    StateEntry("Area32_RunA", ::bof3::Img(Area32_StatesA_addr), at::kArea32States, Sprite_Current[4])();
}

// original 0x4038E0 (Area32_StatesA[0]; PSX 0x801F3728): Sprite_ShadeFadeBegin; the object's
// +0x5C = 1, its x +0x34 moved by its step +0xC, state 1; the leader's word
// +0x12E less 2.
extern "C" void __cdecl Area32_ShadeStart(void) {
    AH_CALL(Sprite_ShadeFadeBegin)();
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    cur[0x5C] = 1;
    AddLong(cur + 0x34, tCur->step_x);
    cur[4] = 1;
    AddWord(Field_State + 0x12E, -2);
}

// original 0x403920 (Area32_StatesA[1]; PSX 0x801F378C): Sprite_ShadeLower(8); while it
// answers 0, the leader's +0x12E less 2 and the object moved by its step;
// once it answers, state 0.
extern "C" void __cdecl Area32_ShadeStep(void) {
    if (AH_CALL(Sprite_ShadeLower)(8) != 0) {
        Sprite_Current[4] = 0;
        return;
    }
    AddWord(Field_State + 0x12E, -2);
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    AddLong(cur + 0x34, tCur->step_x);
}

// original 0x403960 (Area32_Choices[3] = Area32_Handlers[1]; PSX
// 0x801F3800): Area32_StatesB by Sprite_Current[4].
extern "C" void __cdecl Area32_RunB(void) {
    StateEntry("Area32_RunB", ::bof3::Img(Area32_StatesB_addr), at::kArea32States, Sprite_Current[4])();
}

// original 0x403980 (Area32_StatesB[0]; PSX 0x801F3844): Sprite_SetTint(the object, 0, 0,
// 0, 1)'s slot to the active member's +0x9F; the object's +0 bit 0x20 set
// and 0x40 cleared, +0x5C = 1, the tint bytes +0x5F / +0x5E / +0x5D = 0x80,
// step +0xC = 0x2000 and x +0x34 moved by it, state 1; the active member's
// word +0x8A less 2.
extern "C" void __cdecl Area32_TintStart(void) {
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Sprite_Current, 0, 0, 0, 1);
    Field_ActiveMember[0x9F] = tint;
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    cur[0] |= 0x20;
    cur[0] &= 0xBF;
    cur[0x5C] = 1;
    cur[0x5F] = 0x80;
    cur[0x5E] = 0x80;
    cur[0x5D] = 0x80;
    tCur->step_x = 0x2000;
    AddLong(cur + 0x34, tCur->step_x);
    cur[4] = 1;
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x403A20 (Area32_StatesB[1]; PSX 0x801F3920): each tint byte +0x5D, +0x5E, +0x5F
// below 0xC0 as a signed byte gains 4; when all three are exactly 0xC0,
// Tint_Release(the active member's +0x9F), the object's bit 0x20 cleared,
// +0x5C and the tint bytes 0, state 0, the active member's +0x84 = 2;
// otherwise the object moves by its step and the member's +0x8A less 2.
extern "C" void __cdecl Area32_TintStep(void) {
    unsigned char* cur = Sprite_Current;
    auto* tCur = rv::As<records::SpriteObject>(cur);
    for (unsigned off = 0x5D; off <= 0x5F; ++off)
        if (static_cast<signed char>(cur[off]) < static_cast<signed char>(0xC0)) cur[off] = static_cast<unsigned char>(cur[off] + 4);
    if (cur[0x5D] == 0xC0 && cur[0x5E] == 0xC0 && cur[0x5F] == 0xC0) {
        AH_CALL(Tint_Release)(Field_ActiveMember[0x9F]);
        cur = Sprite_Current;
        tCur = rv::As<records::SpriteObject>(cur);
        cur[0] &= 0xDF;
        cur[0x5C] = 0;
        cur[0x5F] = 0;
        cur[0x5E] = 0;
        cur[0x5D] = 0;
        cur[4] = 0;
        Field_ActiveMember[0x84] = 2;
        return;
    }
    AddLong(cur + 0x34, tCur->step_x);
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x403AF0 (Area32_Choices[4] = Area32_Handlers[2]; PSX
// 0x801F3A78): an effect of kind 0x50 at the object, 0x4000 up, timer 0x78.
extern "C" void __cdecl Area32_SpawnEffect50Near(void) { SpawnEffect50(0x4000, 0x78); }

// original 0x403B90 (Area32_Choices[5] = Area32_Handlers[3]; PSX
// 0x801F3C18): an effect of kind 0x4F whose +0xB is the active member's
// index, (Field_ActiveMember - Sprite_Objects) / 0xA4 as a signed division.
extern "C" void __cdecl Area32_SpawnEffect4F(void) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return;
    const auto from = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) - ::bof3::Img(Sprite_Objects_addr));
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 0x4F;
    EffectAt(cur[0xB])[0xB] = static_cast<unsigned char>(from / 0xA4);
}

// original 0x403C00 (Area32_Choices[6] = Area32_Handlers[4]; PSX
// 0x801F3D28): an effect of kind 0x4E.
extern "C" void __cdecl Area32_SpawnEffect4E(void) { SpawnFromCurrent(0x4E); }

// original 0x403C40 (Area32_Choices[7] = Area32_Handlers[5]; PSX
// 0x801F3DCC): an effect of kind 0x50 at the object, 0x8000 up, timer 0x3C.
extern "C" void __cdecl Area32_SpawnEffect50Far(void) { SpawnEffect50(0x8000, 0x3C); }

void Area32_Inject() {
    BOF3_INJECT(Area32_ChoiceLeaderAnim);
    BOF3_INJECT(Area32_RunA);
    BOF3_INJECT(Area32_ShadeStart);
    BOF3_INJECT(Area32_ShadeStep);
    BOF3_INJECT(Area32_RunB);
    BOF3_INJECT(Area32_TintStart);
    BOF3_INJECT(Area32_TintStep);
    BOF3_INJECT(Area32_SpawnEffect50Near);
    BOF3_INJECT(Area32_SpawnEffect4F);
    BOF3_INJECT(Area32_SpawnEffect4E);
    BOF3_INJECT(Area32_SpawnEffect50Far);
}
