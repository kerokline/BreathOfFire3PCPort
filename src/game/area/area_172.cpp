// Moved here by tools/move_funcs.py from area_w4b.cpp (docs/source-layout-round.md).
#include "game/area/area_172.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4b::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// ObjTrio record `m` (0x14C bytes; not checked, as the originals' index).
unsigned char* PartyRecord(unsigned m) { return Mem(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

// A word in [lo, lo + 3), compared as the originals' `sub; cmp r16, 3; jb`.
bool In3(std::uint16_t v, unsigned lo) { return static_cast<std::uint16_t>(v - lo) < 3; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* Story() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the entries that are code (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past the %u code entries of its table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed: ScriptFlags_Set40, then kind and state.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The tail ended: kind and state 0.
void EndTail() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The map byte setter, as the originals pass it: x and z, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// Areas 171 and 172's spawns of an effect of kind 0x92 (0x427600, 0x427FA0):
// a free slot (none: nothing); the record live, kind 0x92, word +0x2E 0, word
// +0x30 `y`, +0x29 `slot29`, +0xB the active member's index among
// Sprite_Objects (its distance over 0xA4, signed, truncated; read after the
// search).
void SpawnEffect92(unsigned short y, unsigned char slot29) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    const auto distance = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) - ::bof3::Img(Sprite_Objects_addr));
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x92;
    tE->screen_x = 0;
    tE->screen_y = y;
    e[0x29] = slot29;
    e[0xB] = static_cast<unsigned char>(distance / static_cast<std::int32_t>(at::kObjectStride));
}

// Area 172's two draws' prim byte stores and float vertices.
void SetFloatInt(unsigned char* p, int v) { SetFloat(p, static_cast<float>(v)); }

}  // namespace

// The 64 x 32 panel's x for effect kind 0xA5's two states: 0xDC, and under
// DIV-0041 0xDC + Widescreen_Anchor() - anchored to the right edge, keeping
// the original's 36 columns to it. The PSP moved it (220, 200) -> (256, 190)
// (psp-widescreen-placements.md 2.6): +36 for its 32 columns, near enough the
// same anchor, and -10 rows to keep it inside its 216-row crop, which ours
// does not have - so y stays 0xC8. 0xDC unarmed or narrow.
static int PanelX() { return 0xDC + Widescreen_Anchor(); }

// original 0x427BB0 (area 172 +0x3C[0] = +0x34[1]; PSX 0x801F4324): story
// flag 0x4E cleared.
extern "C" void __cdecl Area172_ClearFlag4E(void) { AH_CALL(Flags_Clear)(Story(), 0x4E); }

// original 0x427BC0 (area 172 +0x3C[1] = +0x34[2]; PSX 0x801F434C): the first
// member (below Field_MemberCount, read once) whose +0x89 is 4: the script
// position + 3.
extern "C" void __cdecl Area172_SkipIfMember89Is4(void) {
    const unsigned count = Field_MemberCount;
    for (unsigned m = 0; m < count; m = (m + 1) & 0xFF) {
        if (PartyRecord(m)[0x89] == 4) {
            AddWord(MoveScript_Object + 0xA, 3);
            return;
        }
    }
}

// original 0x427C10 (area 172 +0x3C[2] = +0x34[3]; PSX 0x801F43C8):
// Area172_FallStates by Sprite_Current[4]. The table's two entries run on
// into Area172_SlideStates' two (code: the fall's state 1 sets 2), then the
// drift steps (data): ours aborts at 4.
extern "C" void __cdecl Area172_RunFall(void) {
    StateEntry("Area172_RunFall", ::bof3::Img(Area172_FallStates_addr), at::kArea172FallReach, Sprite_Current[4])();
}

// original 0x427C30 (Area172_FallStates[0]): the running object's +0 bit
// 0x40 cleared and 0x20 set, the tint +0x5C 1 and +0x5D..+0x5F 0x80, the
// velocities +0xC, +0x10, +0x14 0, +0x20 -8, state 1; the script position - 2.
extern "C" void __cdecl Area172_FallStart(void) {
    unsigned char* const o = Sprite_Current;
    auto* const tO = rv::As<records::SpriteObject>(o);
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    o[0] = static_cast<unsigned char>(o[0] | 0x20);
    o[0x5C] = 1;
    o[0x5D] = 0x80;
    o[0x5E] = 0x80;
    o[0x5F] = 0x80;
    tO->step_x = 0;
    tO->step_z = 0;
    tO->step_y = 0;
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(o)->work_20, -8);
    o[4] = 1;
    AddWord(MoveScript_Object + 0xA, -2);
}

// original 0x427CB0 (Area172_FallStates[1]): +0x14 += +0x20;
// Field_LeaderStepTick; the ground at the object (MapView_GroundAt) above its
// height word +0x3E (16 bits, signed): landed - +0 bit 0x20 cleared, the tint
// 0, the height the ground, +0x14 and +0x20 0, state 2,
// Sprite_SetAnimation(0x39), the script object's +1 = 4; else the script
// position - 2. Then the field record's tint record (+0x149's) bytes +2..+4
// less 2; the object's +0x5D not 0x40: +0x5D..+0x5F + 4. Sprite_Kind2's
// height dword = the object's +0x3C, MapView_SetElevation(its word +0x3E).
extern "C" void __cdecl Area172_FallStep(void) {
    unsigned char* o = Sprite_Current;
    auto* tO = rv::As<records::SpriteObject>(o);
    tO->step_y = static_cast<std::int32_t>(static_cast<U>(tO->step_y) + static_cast<U>(rv::As<records::SpriteObject>(o)->work_20));
    AH_CALL(Field_LeaderStepTick)();
    o = Sprite_Current;
    tO = rv::As<records::SpriteObject>(o);
    const long ground = AH_CALL(MapView_GroundAt)(tO->x, tO->z);
    o = Sprite_Current;
    tO = rv::As<records::SpriteObject>(o);
    if (static_cast<std::int16_t>(ground) > static_cast<std::int16_t>(rv::As<records::SpriteObject>(o)->height_whole)) {
        o[0] = static_cast<unsigned char>(o[0] & 0xDF);
        o[0x5C] = 0;
        o[0x5D] = 0;
        o[0x5E] = 0;
        o[0x5F] = 0;
        rv::Store<unsigned>(rv::As<records::SpriteObject>(o)->height_whole, static_cast<unsigned>(ground));
        tO->step_y = 0;
        rv::Store<std::int32_t>(rv::As<records::SpriteObject>(o)->work_20, 0);
        o[4] = 2;
        AH_CALL(Sprite_SetAnimation)(0x39);
        MoveScript_Object[1] = 4;
    } else {
        AddWord(MoveScript_Object + 0xA, -2);
    }
    const unsigned char* const field = Field_State;
    unsigned char* const tint = Mem(::bof3::Img(MoveScript_TintRecords_addr) + field[at::kFieldStateTint] * at::kTintStride);
    tint[2] = static_cast<unsigned char>(tint[2] + 0xFE);
    tint[3] = static_cast<unsigned char>(tint[3] + 0xFE);
    tint[4] = static_cast<unsigned char>(tint[4] + 0xFE);
    o = Sprite_Current;
    tO = rv::As<records::SpriteObject>(o);
    if (o[0x5D] != 0x40) {
        o[0x5D] = static_cast<unsigned char>(o[0x5D] + 4);
        o[0x5E] = static_cast<unsigned char>(o[0x5E] + 4);
        o[0x5F] = static_cast<unsigned char>(o[0x5F] + 4);
    }
    SetLong(Mem(::bof3::Img(Sprite_Kind2_addr) + 0x3c), tO->height);
    AH_CALL(MapView_SetElevation)(static_cast<int>(static_cast<std::uint16_t>(rv::As<records::SpriteObject>(o)->height_whole)));
}

// original 0x427DF0 (area 172 +0x3C[3] = +0x34[4]; PSX 0x801F471C):
// Area172_SlideStates by Sprite_Current[4]; past its two entries is data
// (the drift steps): ours aborts.
extern "C" void __cdecl Area172_RunSlide(void) {
    StateEntry("Area172_RunSlide", ::bof3::Img(Area172_SlideStates_addr), at::kArea172SlideCount, Sprite_Current[4])();
}

// original 0x427E10 (Area172_SlideStates[0]): +0 bit 0x20 set and 0x40
// cleared, the tint +0x5C 1 and +0x5D..+0x5F 0x80; Area172_SlideStep once;
// state 1.
extern "C" void __cdecl Area172_SlideStart(void) {
    unsigned char* const o = Sprite_Current;
    o[0] = static_cast<unsigned char>(o[0] | 0x20);
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    o[0x5C] = 1;
    o[0x5F] = 0x80;
    o[0x5E] = 0x80;
    o[0x5D] = 0x80;
    AH_CALL(Area172_SlideStep)();
    Sprite_Current[4] = 1;
}

// original 0x427E60 (Area172_SlideStates[1]; called by Area172_SlideStart):
// x += +0xC, z += +0x10; each of +0x5D..+0x5F below 0xC0 as a signed byte +
// 2; all three 0xC0: +0 bit 0x20 cleared, the tint and the state 0; else the
// active member's word +0x8A - 2.
extern "C" void __cdecl Area172_SlideStep(void) {
    unsigned char* const o = Sprite_Current;
    auto* const tO = rv::As<records::SpriteObject>(o);
    tO->x = static_cast<std::int32_t>(static_cast<U>(tO->x) + static_cast<U>(tO->step_x));
    tO->z = static_cast<std::int32_t>(static_cast<U>(tO->z) + static_cast<U>(tO->step_z));
    for (unsigned k = 0x5D; k <= 0x5F; ++k)
        if (static_cast<signed char>(o[k]) < static_cast<signed char>(0xC0)) o[k] = static_cast<unsigned char>(o[k] + 2);
    if (o[0x5D] == 0xC0 && o[0x5E] == 0xC0 && o[0x5F] == 0xC0) {
        o[0] = static_cast<unsigned char>(o[0] & 0xDF);
        o[0x5C] = 0;
        o[0x5F] = 0;
        o[0x5E] = 0;
        o[0x5D] = 0;
        o[4] = 0;
        return;
    }
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x427F20 (area 172 +0x3C[4] = +0x34[5]; PSX 0x801F491C): counter 0
// below 7: x += Area172_DriftSteps[+0xA & 0xF] << 11 and z -= it << 11 (the
// step signed), +0xA - 1, the field record's word +0x12E - 2; else x and z
// snapped to a half cell (& 0xFFFF8000).
extern "C" void __cdecl Area172_Drift(void) {
    unsigned char* const o = Sprite_Current;
    auto* const tO = rv::As<records::SpriteObject>(o);
    if (B(::bof3::Img(at::kCounter0.v)) < 7) {
        U step = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(Area172_DriftSteps_addr) + (o[0xA] & 0xF)))));
        tO->x = static_cast<std::int32_t>(static_cast<U>(tO->x) + (step << 11));
        step = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(Area172_DriftSteps_addr) + (o[0xA] & 0xF)))));
        tO->z = static_cast<std::int32_t>(static_cast<U>(tO->z) + ((0u - step) << 11));
        o[0xA] = static_cast<unsigned char>(o[0xA] - 1);
        AddWord(Field_State + 0x12E, -2);
        return;
    }
    tO->x = static_cast<std::int32_t>(static_cast<U>(tO->x) & 0xFFFF8000u);
    tO->z = static_cast<std::int32_t>(static_cast<U>(tO->z) & 0xFFFF8000u);
}

// original 0x427FA0 (area 172 +0x3C[5] = +0x34[6]; PSX 0x801F49DC): an effect
// of kind 0x92 (+0x30 0, +0x29 4).
extern "C" void __cdecl Area172_SpawnEffect92(void) { SpawnEffect92(0, 4); }

// original 0x428010 (area 172 +0x3C[6] = +0x34[7]; PSX 0x801F4AAC): +0 bit
// 0x20 set, the tint +0x5C 1 and +0x5D..+0x5F 0xC0.
extern "C" void __cdecl Area172_TintOn(void) {
    unsigned char* const o = Sprite_Current;
    o[0] = static_cast<unsigned char>(o[0] | 0x20);
    o[0x5C] = 1;
    o[0x5F] = 0xC0;
    o[0x5E] = 0xC0;
    o[0x5D] = 0xC0;
}

// original 0x428050 (area 172 +0x3C[7] = +0x34[8]; PSX 0x801F4AF4): +0 bit
// 0x20 cleared, the tint 0.
extern "C" void __cdecl Area172_TintOff(void) {
    unsigned char* const o = Sprite_Current;
    o[0] = static_cast<unsigned char>(o[0] & 0xDF);
    o[0x5C] = 0;
    o[0x5F] = 0;
    o[0x5E] = 0;
    o[0x5D] = 0;
}

// original 0x428090 (area 172 +0x34[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, then Cond row 14's flag 0x12 set - tail kind 35 at state
// 5; clear - MoveScript_Var7 6 and the byte after it 0 (the tail not
// written).
extern "C" void __cdecl Area172_ChoiceFlag12(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x70), 0x12) != 0) {
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x23;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 5;
        return;
    }
    B(::bof3::Img(MoveScript_Var7_addr)) = 6;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0;
}

// original 0x4280E0 (tail kind 35): 0: Party_DropIn(1), state 1; 1: counter 3
// at 0x14 - story flag 0x4E, Field_ChangeArea(0xAC, 0xA0000, 0x50000, 0x83),
// the tail ended; 5: Party_DropIn(0), state 6; 6: counter 3 at 0x14 - flag
// 0x4E, Field_ChangeArea(0xAC, 0x260000, 0x360000, 0x82), ended. Any other
// state: nothing.
extern "C" void __cdecl Area172_Tail35(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        AH_CALL(Party_DropIn)(1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 1:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x14) return;
        AH_CALL(Flags_Set)(Story(), 0x4E);
        AH_CALL(Field_ChangeArea)(0xAC, 0xA0000, 0x50000, 0x83);
        EndTail();
        return;
    case 5:
        AH_CALL(Party_DropIn)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 6;
        return;
    case 6:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x14) return;
        AH_CALL(Flags_Set)(Story(), 0x4E);
        AH_CALL(Field_ChangeArea)(0xAC, 0x260000, 0x360000, 0x82);
        EndTail();
        return;
    default: return;
    }
}

// original 0x4281A0 (Area_StepHook's case for area 172, 0x56E1E2):
// Cond_ByteFD 0, x exactly 0x248000, z's high word 0x35..0x37 and the
// leader's pose 6, 7 or 0: tail kind 35 armed at 0, al 1; else al 0.
extern "C" unsigned char __cdecl Area172_StepHook(long x, long z) {
    if (Cond_ByteFD != 0) return 0;
    if (static_cast<U>(x) != 0x248000) return 0;
    if (!In3(High(z), 0x35)) return 0;
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    if (pose != 6 && pose != 7 && pose != 0) return 0;
    ArmTail(0x23, 0);
    return 1;
}

// original 0x4281F0 (area 172 +0x40; PSX 0x801F4D58): map cell (9, 3) 0x50.
extern "C" void __cdecl Area172_InitCell(void) { SetByte(9, 3, 0x50); }

// original 0x428200 (Effect_KindHandlers[0xA5]; a gap of the tool):
// Area172_EffectStates by Sprite_Current[1] (the running effect record);
// past its three entries is data: ours aborts.
extern "C" void __cdecl Area172_EffectA5Run(void) {
    StateEntry("Area172_EffectA5Run", ::bof3::Img(Area172_EffectStates_addr), at::kArea172EffectCount, Sprite_Current[1])();
}

// original 0x428220 (Area172_EffectStates[0]): the record's word +0x2E and
// dword +0xC 0, state 1.
extern "C" void __cdecl Area172_EffectA5Start(void) {
    unsigned char* const e = Sprite_Current;
    auto* const tE = rv::As<records::EffectObject>(e);
    tE->screen_x = 0;
    tE->step_x = 0;
    e[1] = 1;
}

// original 0x428240 (Area172_EffectStates[1]): the word +0x2E + 10; above
// 0x190 (signed): the byte after MoveScript_Var7 + 1 and state 2. Then
// Area172_DrawPanel(0xDC, 0xC8) and Area172_DrawShade(+0x2E, 0xC8) (the word
// read again after the first draw). DIV-0041: the panel's x is PanelX(); the
// shade's x is drawn mapped onto the wide picture (Widescreen_SweepShadeX, I38:
// its start past the wide left edge, anchored right with the panel from 0xDC
// on), the word +0x2E and the > 0x190 test Capcom's - the same int when narrow.
extern "C" void __cdecl Area172_EffectA5Grow(void) {
    unsigned char* const e = Sprite_Current;
    auto* const tE = rv::As<records::EffectObject>(e);
    AddWord(e + 0x2E, 10);
    if (static_cast<std::int16_t>(static_cast<std::uint16_t>(tE->screen_x)) > 0x190) {
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = static_cast<unsigned char>(B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) + 1);
        e[1] = 2;
    }
    AH_CALL(Area172_DrawPanel)(PanelX(), 0xC8);
    AH_CALL(Area172_DrawShade)(Widescreen_SweepShadeX(static_cast<int>(static_cast<std::uint16_t>(rv::Current<records::EffectObject>()->screen_x))), 0xC8);
}

// original 0x428290 (Area172_EffectStates[2]): Area172_DrawPanel(0xDC, 0xC8);
// DIV-0041: the x is PanelX().
extern "C" void __cdecl Area172_EffectA5Hold(void) { AH_CALL(Area172_DrawPanel)(PanelX(), 0xC8); }

// original 0x4282B0 (called by effect kind 0xA5's states 1 and 2): a POLY_FT4
// at Gfx_PacketNext, the arguments' low words signed: (x, y)..(x + 0x40, y +
// 0x20) as floats, u 0..0x40, v 0..0x20, clut (0, 0x1EB), tpage (1, 1, 0x300,
// 0x100), colour 0x80 grey; committed to slot 1 (0x48 bytes).
extern "C" void __cdecl Area172_DrawPanel(int x, int y) {
    unsigned char* const p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    const int sx = static_cast<std::int16_t>(x);
    const int sy = static_cast<std::int16_t>(y);
    SetFloatInt(p + 8, sx);
    p[0x24] = 0x40;
    p[0x44] = 0x40;
    SetFloatInt(p + 0xC, sy);
    p[0x35] = 0x20;
    p[0x45] = 0x20;
    SetFloatInt(p + 0x18, sx + 0x40);
    SetFloatInt(p + 0x1C, sy);
    SetFloatInt(p + 0x38, sx + 0x40);
    SetFloatInt(p + 0x28, sx);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x25] = 0;
    SetFloatInt(p + 0x2C, sy + 0x20);
    SetFloatInt(p + 0x3C, sy + 0x20);
    p[0x34] = 0;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, AH_CALL(Gpu_GetClut)(0, 0x1EB));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, AH_CALL(Gpu_GetTPage)(1, 1, 0x300, 0x100));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x428370 (called by effect kind 0xA5's state 1): a draw-mode packet
// (dtd 1, tpage (0, 2, 0x3C0, 0), tw 0 - the fifth word is a push the
// original leaves from the tpage call) committed to slot 1 (0xC); then a
// POLY_G4 at Gfx_PacketNext (read again), the arguments' low words signed:
// (x - 0x40, y) black, (320, y) white, (x, y + 0x30) grey 0x40, (320, y +
// 0x30) white, as floats; semi-transparent, shade-tex 0; committed to slot 1
// (0x44 bytes).
extern "C" void __cdecl Area172_DrawShade(int x, int y) {
    const unsigned tpage = AH_CALL(Gpu_GetTPage)(0, 2, 0x3C0, 0) & 0xFFFF;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(p);
    const int sx = static_cast<std::int16_t>(x);
    const int sy = static_cast<std::int16_t>(y);
    // DIV-0041: the right edge at 373 under the wide picture (widescreen.h: 0
    // until every self-test has run); the left, at x - 0x40, is the caller's.
    const auto k320 = std::bit_cast<std::int32_t>(320.0f + static_cast<float>(Widescreen_Fill()));   // 0x43A00000 narrow
    SetLong(p + 0x38, k320);
    SetLong(p + 0x18, k320);
    SetFloatInt(p + 8, sx - 0x40);
    SetFloatInt(p + 0x28, sx);
    p[4] = 0;
    SetFloatInt(p + 0x1C, sy);
    SetFloatInt(p + 0xC, sy);
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0xFF;
    p[0x15] = 0xFF;
    SetFloatInt(p + 0x3C, sy + 0x30);
    SetFloatInt(p + 0x2C, sy + 0x30);
    p[0x16] = 0xFF;
    p[0x24] = 0x40;
    p[0x25] = 0x40;
    p[0x26] = 0x40;
    p[0x34] = 0xFF;
    p[0x35] = 0xFF;
    p[0x36] = 0xFF;
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    AH_CALL(Gpu_SetShadeTex)(p, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0x44);
}

void Area172_Inject() {
    BOF3_INJECT(Area172_ClearFlag4E);
    BOF3_INJECT(Area172_SkipIfMember89Is4);
    BOF3_INJECT(Area172_RunFall);
    BOF3_INJECT(Area172_FallStart);
    BOF3_INJECT(Area172_FallStep);
    BOF3_INJECT(Area172_RunSlide);
    BOF3_INJECT(Area172_SlideStart);
    BOF3_INJECT(Area172_SlideStep);
    BOF3_INJECT(Area172_Drift);
    BOF3_INJECT(Area172_SpawnEffect92);
    BOF3_INJECT(Area172_TintOn);
    BOF3_INJECT(Area172_TintOff);
    BOF3_INJECT(Area172_ChoiceFlag12);
    BOF3_INJECT(Area172_Tail35);
    BOF3_INJECT(Area172_StepHook);
    BOF3_INJECT(Area172_InitCell);
    BOF3_INJECT(Area172_EffectA5Run);
    BOF3_INJECT(Area172_EffectA5Start);
    BOF3_INJECT(Area172_EffectA5Grow);
    BOF3_INJECT(Area172_EffectA5Hold);
    BOF3_INJECT(Area172_DrawPanel);
    BOF3_INJECT(Area172_DrawShade);
}
