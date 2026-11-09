// Moved here by tools/move_funcs.py from area_w1a.cpp (docs/source-layout-round.md).
#include "game/area/area_39.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1a.h"
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

using Handler = void (__cdecl*)();

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// Field_ActiveMember's word +0x8A less 2 (area 39's handlers).
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }

void SetByte(U x, U z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

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
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    if (cur[0xB] == 0xFF) return;
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 6;
    EffectAt(cur[0xB])[6] = 3;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->step_x, 0);
    const auto step = static_cast<signed char>(B(::bof3::Img(Area38_EffectBytes_addr) + B(list)));
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->step_z, step);
    rv::Store<unsigned>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->screen_x, static_cast<std::uint16_t>(tCur->screen_x));
    rv::Store<unsigned>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->screen_y, static_cast<std::uint16_t>(tCur->screen_y));
}

}  // namespace

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
extern "C" void __cdecl Area38_SpawnEffectMember1(void) { SpawnForMember(::bof3::Img(Cond_Flags_addr) + 0xd3, ::bof3::Img(ObjTrio_addr) + 0x14c); }

// original 0x4054D0 (Area38_Handlers[3]; PSX 0x801F2E60): 0x904064, record 2.
extern "C" void __cdecl Area38_SpawnEffectMember2(void) { SpawnForMember(::bof3::Img(Cond_Flags_addr) + 0xd4, ::bof3::Img(ObjTrio_addr) + 0x298); }

// original 0x405590 (Area39_Handlers[0]; PSX 0x801F2C04): Area39_States by
// Sprite_Current[4].
extern "C" void __cdecl Area39_Run(void) {
    StateEntry("Area39_Run", ::bof3::Img(Area39_States_addr), at::kArea39StateCount, Sprite_Current[4])();
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
    AddLong(cur + 0x34, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(Area39_DriftSteps_addr) + (cur[0xA] & 0xF))))) << 11);
    cur = Sprite_Current;
    const auto step = static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(Area39_DriftSteps_addr) + (cur[0xA] & 0xF))));
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
extern "C" void __cdecl Area39_SetScriptFlag8(void) { B(::bof3::Img(Field_ScriptFlags_addr)) = static_cast<unsigned char>(B(::bof3::Img(Field_ScriptFlags_addr)) | 8); }

// original 0x405720 (Area39_Handlers[12]; PSX 0x801F2F0C): the first key-item
// byte of 0xE becomes 4; none: 0x591900(4), the key item 4 added.
extern "C" void __cdecl Area39_SwapKeyItemE(void) {
    for (unsigned i = 0; i < at::kKeyItemCount; ++i)
        if (B(::bof3::Img(at::kKeyItems.v) + i) == 0xE) {
            B(::bof3::Img(at::kKeyItems.v) + i) = 4;
            return;
        }
    AH_AT(void (__cdecl*)(unsigned), at::kKeyItemAdd.v)(4);
}

// original 0x405750 (Area39_Handlers[13], also area 130's +0x3C[4]; PSX
// 0x801F2F68).
extern "C" void __cdecl Area39_FadeOutMusic(void) { AH_CALL(Music_FadeOutStop)(0x1E); }

// original 0x405760 (Area39_Handlers[14]; PSX 0x801F2F94): counter 1 = the
// first party-list byte is not 4.
extern "C" void __cdecl Area39_Counter1Not4(void) { B(::bof3::Img(at::kCounter1.v)) = B(::bof3::Img(Cond_Flags_addr) + 0xd2) != 4 ? 1 : 0; }

void Area39_Inject() {
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
}
