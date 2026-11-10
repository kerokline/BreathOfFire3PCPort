// Moved here by tools/move_funcs.py from area_w2b.cpp (docs/source-layout-round.md).
#include "game/area/area_85.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2b_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::SetWord;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char* PartyRecord(U index) { return At(::bof3::Img(ObjTrio_addr) + index * at::kPartyStride); }

// Area 85's shift of CLUT row 6, called by its init by the original address.
constexpr ::bof3::image::CodeVa kClutShift85{bof3::addr::Area85_ClutShift};

// Area 86's member test, called by its cell hook.
constexpr ::bof3::image::CodeVa kMemberNear86{bof3::addr::Area86_MemberNear};

using ShiftFn = void (__cdecl*)(int);

using NearFn = unsigned char (__cdecl*)(unsigned, unsigned);

using ObjectFn = void (__cdecl*)(unsigned char*);

using SlotFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*);

using BitsFn = void (__cdecl*)(unsigned char*, unsigned);

using ValueFn = void (__cdecl*)(unsigned);

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
        return rv::Bytes(&Effect_Objects[o[0xB]]);
    };
    record()[0] = 1;
    record()[5] = 0x47;
    record()[0xB] = sub;
    AH_CALL(Sound_PlayEffect)(0x200);
}

}  // namespace

// original 0x40F720 (area 85's handler 0, Area85_Handlers 0x61177C): the
// kind-2 object's step count MoveScript_Object[7] = (Sprite_Current +0x34 -
// 0x160000) >> 15 (arithmetic, a byte); MoveCmd_MoveKind2(7); then
// Sprite_Current (read again) +0x14 = 0, MoveScript_FAWord = 0,
// MoveScript_Object (read again) +0 |= 0x40, Field_Kind2Z = 0x3E0000 and
// Sprite_Current +0x38 = 0x3E0000.
extern "C" void __cdecl Area85_Kind2Walk(void) {
    MoveScript_Object[7] = static_cast<unsigned char>(static_cast<U>((rv::Current<records::SpriteObject>()->x - 0x160000) >> 15));
    AH_CALL(MoveCmd_MoveKind2)(7);
    rv::Current<records::SpriteObject>()->step_y = 0;
    MoveScript_FAWord = 0;
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Field_Kind2Z = 0x3E0000;
    rv::Current<records::SpriteObject>()->z = 0x3E0000;
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
    if (static_cast<unsigned char>(MoveScript_Var7) == 6 && MoveScript_PartyRecords[0x65] == 0xC) {
        Cur()[0] = static_cast<unsigned char>(Cur()[0] & 0xBF);
        AH_CALL(Field_MemberSprite)(2, 2);
        AH_CALL(Sprite_SetAnimation)(1);
        return;
    }
    for (unsigned char i = 0; i < Field_MemberCount; ++i) {
        const unsigned char* const record = PartyRecord(i);
        const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
        if (record[0x89] != 2) continue;
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, tRecord->x);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, tRecord->z);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, tRecord->height);
    }
    move_script::SetPos(MoveScript_Object, move_script::Pos(MoveScript_Object) + 0xFFFEu);
}

// original 0x40F830 (area 85's handler 2): Sprite_Current +6 = 7,
// Field_ActiveMember +0x9E = 0.
extern "C" void __cdecl Area85_SetType7(void) {
    Cur()[6] = 7;
    Field_ActiveMember[0x9E] = 0;
}

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
    AH_AT(ObjectFn, area_w2b::kSlotsReleaseFor.v)(Cur());
    AH_AT(SlotFn, area_w2b::kSlotStart.v)(Cur(), At(::bof3::Img(at::kA85SlotScript.v)));
    AH_CALL(Sprite_SetAnimation)(0);
}

// original 0x40F990 (area 85's handler 7, and area 198's handler 3: one body,
// in area 85's block): 0x454A80(Sprite_Current) - the object's Field_Slots
// scripts released.
extern "C" void __cdecl Area85_ReleaseSlots(void) { AH_AT(ObjectFn, area_w2b::kSlotsReleaseFor.v)(Cur()); }

// original 0x40F9A0 (area 85's handler 8): the word Sprite_Current +0x3E +=
// 0x18.
extern "C" void __cdecl Area85_Raise18(void) { rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, rv::Current<records::SpriteObject>()->height_whole + 0x18u); }

// original 0x40F9B0 (area 85's handler 9): Sound_PlayEffect(0x201) unless
// Field_Request is 5.
extern "C" void __cdecl Area85_Sound201(void) {
    if (Field_Request != 5) AH_CALL(Sound_PlayEffect)(0x201);
}

// original 0x40F9D0 (area 85's init, its descriptor 0x6117A8's +0x40):
// Area85_ClutShift(-8).
extern "C" void __cdecl Area85_Init(void) { AH_AT(ShiftFn, kClutShift85.v)(-8); }

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
        const U word = Word(At(::bof3::Img(Gfx_ClutStripSource_addr) + 0xc00 + n * 2));
        const U r = channel(word & 0x1F);
        const U g = channel((word >> 5) & 0x1F);
        const U b = channel((word >> 10) & 0x1F);
        SetWord(At(::bof3::Img(Gfx_ClutStrip_addr) + 0xc00 + n * 2), (((b << 5) | g) << 5) | (word & 0x8000) | r);
    }
    Gfx_ClutStripDirty = 1;
}

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
    for (U a = ::bof3::Img(Area86_Switches_addr) + 1; a < ::bof3::Img(at::kA86SwitchesEnd.v) + 1; a += 4, ++k) {
        if (At(a - 1)[0] == (x & 0xFF) && At(a)[0] == (z & 0xFF) && At(a + 1)[0] == ObjTrio[0].direction) break;
    }
    if (k == 3) return 0;
    const unsigned char* const rect = At(::bof3::Img(Area86_Rects_addr) + k * 4);
    for (std::int32_t cx = rect[0]; cx < rect[2]; ++cx) {
        for (std::int32_t cz = rect[1]; cz < rect[3]; ++cz) {
            const auto near = static_cast<signed char>(AH_AT(NearFn, kMemberNear86.v)(static_cast<unsigned>(cx), static_cast<unsigned>(cz)));
            if (near >= 0) return 0;
        }
    }
    if (AH_CALL(Flags_Test)(Bank(), 0x1C) != 0) return 0;
    AH_AT(BitsFn, area_w2b::kFlagsToggle.v)(Bank(), At(::bof3::Img(Area86_Switches_addr) + 3 + k * 4)[0]);
    AH_CALL(Sound_PlayEffect)(0x200);
    AH_AT(ValueFn, area_w2b::kSpawnKind4.v)(0xF);
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
        const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
        if (record[0] == 0) continue;
        const U steps = record[9];
        const auto reach = static_cast<std::int32_t>((static_cast<U>(record[0x70]) + 2) << 15);
        const auto dx = static_cast<std::int32_t>(static_cast<U>(tRecord->step_x) * steps - (x << 16) + static_cast<U>(tRecord->x));
        const auto ax = static_cast<std::int32_t>(dx < 0 ? 0u - static_cast<U>(dx) : static_cast<U>(dx));
        if (ax >= reach) continue;
        const auto dz = static_cast<std::int32_t>(static_cast<U>(tRecord->step_z) * steps - (z << 16) + static_cast<U>(tRecord->z));
        const auto az = static_cast<std::int32_t>(dz < 0 ? 0u - static_cast<U>(dz) : static_cast<U>(dz));
        if (az < reach) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void Area85_Inject() {
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
}
