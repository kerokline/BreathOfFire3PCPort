// The engine callees the chapter and area groups call by raw address and
// nobody owned after group SX - round ten, group SX2, taken with the scenario
// harness (scenario_harness.h). docs/scena_sx2.md has each function, every
// caller, and what was read around them and left.
//
//   Effect_HoldFlag1C        0x469FE0  an effect of kind 4 that holds story flag 0x1C for n frames
//   Party_PlaceInFormation   0x532FD0  Sprite_Current at the formation's offset for a slot, grounded
//   AreaMap_SetHeight        0x572620  one byte of the area block's height layer
//   Flags_Toggle             0x57C160  bits[i >> 3] ^= 1 << (i & 7)
//   Camera_TurnStep          0x57C5A0  Camera_Angles[0] a step toward an angle; Light_AnglesCopy[0] at the end
//   Camera_TurnFBToDegrees   0x57C600  Camera_TurnStepFB with the angle and step in degrees
//   Camera_TurnStepFB        0x57C650  Cond_AngleFB a step toward an angle; Light_AnglesCopy[2] at the end
//   Member_SetState2_8       0x57C8A0  a party member's state 2, sub-state 8, +3 0, +0xB a value
//   Sound_StopChannels       0x587860  every Sound_Channels buffer stopped and forgotten
//   Sound_SetCueVolume       0x587890  the volume of each voice of a sound cue
//   KeyItem_Remove           0x591920  a key item out of the 32-byte list
//   AbilityList_ForType      0x591EC0  the one of a record's four ability lists an id's type files it in
//   Gpu_SetSprt16            0x5A7730  a primitive's code 0x7C (SPRT_16) and depth 0.01
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. The unchecked indexes the
// originals make (a member byte past ObjTrio's three, a formation group past
// its rows, a cue's bank 0) are reproduced (docs/scena_sx2.md section 6).
#include "game/scena_sx2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sx2_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sx2::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// A signed byte over 4 the way the original divides (cdq / and 3 / add / sar
// 2): C's truncating division.
std::int32_t Quarter(unsigned char b) { return static_cast<signed char>(b) / 4; }

// One turn step of a camera angle word (Camera_TurnStep's and
// Camera_TurnStepFB's shared shape, two constants apart): with a step (s8)
// the word plus the step, then while (the dword & 0xFFF) is short of the
// target (as s16, the direction the step's sign gives) al 1; else - or with
// no step - the word the target and the Light_AnglesCopy word the
// Light_Angles dword + the target + the bias, al 0. The answer is al alone:
// the rest of eax is the dword's bits, the Light_Angles dword's or, turning
// back, the caller's own eax (every caller tests al).
unsigned char TurnStep(std::uint32_t angle_cell, std::uint32_t light, std::uint32_t copy, std::int32_t bias,
                       std::uint32_t target, unsigned step_arg) {
    const auto step = static_cast<signed char>(step_arg);
    const auto t = static_cast<short>(target);
    if (step != 0) {
        SetWord(At(angle_cell), static_cast<std::uint32_t>(Word(At(angle_cell)) + static_cast<short>(step)));
        const auto now = static_cast<short>(U(Long(At(angle_cell))) & 0xFFF);
        if (step > 0) {
            if (now < t) return 1;
        } else {
            if (now > t) return 1;
        }
    }
    const std::uint32_t l = U(Long(At(light)));
    SetWord(At(angle_cell), target);
    SetWord(At(copy), l + target + static_cast<std::uint32_t>(bias));
    return 0;
}

}  // namespace

// original 0x469FE0: Effect_FindFree; with a record (al not 0xFF), its +0 1
// (in use), +5 4 (the kind: Effect_KindHandlers[4] = 0x469FB0 EffectKind04_HoldTick, which counts
// +9 down a frame and at 0 clears story flag 0x1C and releases the record),
// +9 the argument's low byte (the frames); then Flags_Set(0x904030, 0x1C).
// No record: nothing, the flag not set either. Areas 49, 77, 86, 112, 117,
// 118 and the engine's 0x4FEEB0 call it, each with 0xF; Area49_CellHook
// answers 0 while flag 0x1C is set (a switch's cool-down). PSX 0x8019BBB8
// (pairs_propagated "gap74").
extern "C" void __cdecl Effect_HoldFlag1C(unsigned frames) {
    const unsigned i = SH_CALL(Effect_FindFree)();
    if (i == 0xFF) return;
    unsigned char* const e = Effect_Objects + i * 0x80;
    e[0] = 1;
    e[5] = 4;
    e[9] = static_cast<unsigned char>(frames);
    SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x1C);
}

// original 0x532FD0: Sprite_Current placed at (x, z) plus the event battle's
// formation offset for a slot, then grounded (Party_PlaceForBattle's one
// callee). The group g is the byte 0x904060, plus 1 at two members, plus 4 at
// three (bytes, wrapping; Field_MemberCount read once); the row is
// Formation_SlotVectors[g * 3 + slot] (slot the third argument's low byte;
// both unchecked), a pair of dwords (dx, dz). The formation byte f (0x904AAC)
// names a pair of s8 at BattleFormation_Offsets[2 f]: with a not 0, +0x34 =
// dx * (a / 4) + x and +0x38 = dz * (a / 4) + z; else +0x34 = x - dz * (b /
// 4) and +0x38 = dx * (b / 4) + z - the vector turned a quarter. Products
// wrap at 32 bits; f and the pair are read again for z. Then +0x3E =
// MapView_GroundAt(+0x34, +0x38)'s word (Sprite_Current read again after the
// call). No PSX twin paired.
extern "C" void __cdecl Party_PlaceInFormation(int x, int z, unsigned slot) {
    const unsigned char count = Field_MemberCount;
    const unsigned char lead = At(at::kLeaderSlot)[0];
    unsigned char g = lead;
    if (count == 2)
        g = static_cast<unsigned char>(lead + 1);
    else if (count == 3)
        g = static_cast<unsigned char>(lead + 4);
    const unsigned char* const pairs = At(bof3::addr::BattleFormation_Offsets);
    const unsigned char* const row = At(bof3::addr::Formation_SlotVectors + (g * 3u + (slot & 0xFF)) * 8);
    const unsigned f = At(at::kFormation)[0];
    const unsigned char a = pairs[f * 2];
    std::uint32_t zd;
    if (a != 0) {
        SetLong(Sprite_Current + 0x34,
                static_cast<std::int32_t>(U(Long(row)) * U(Quarter(a)) + U(x)));
        const unsigned f2 = At(at::kFormation)[0];
        zd = U(Quarter(pairs[f2 * 2])) * U(Long(row + 4));
    } else {
        SetLong(Sprite_Current + 0x34,
                static_cast<std::int32_t>(U(x) - U(Quarter(pairs[f * 2 + 1])) * U(Long(row + 4))));
        const unsigned f2 = At(at::kFormation)[0];
        zd = U(Quarter(pairs[f2 * 2 + 1])) * U(Long(row));
    }
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(zd + U(z)));
    unsigned char* const s = Sprite_Current;
    const long ground = SH_CALL(MapView_GroundAt)(Long(s + 0x34), Long(s + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<std::uint32_t>(ground));
}

// original 0x572620: the height layer's byte of cell (x, z) (s16s) set:
// AreaMap_Header[(AreaMap_Header's dword & 0xFF) * z + AreaMap_HeightBase * 4
// + x] = the value's low byte - AreaMap_Elevation's layer (the width byte
// times z, the base a u16), 32-bit sums, unbounded. eax is left the row
// offset; no caller reads it. Area 52's block puzzle, areas 108 and 135, and
// EffectKind30_ClaimCells / _FreeCells (0x46BF80 / 0x46C100; values 0x10, 0). PSX 0x80155AF4
// (pairs_propagated "call").
extern "C" void __cdecl AreaMap_SetHeight(unsigned x, unsigned z, unsigned value) {
    const std::uint32_t width = AreaMap_Header[0];
    const std::uint32_t row = width * U(static_cast<short>(z)) + static_cast<std::uint32_t>(AreaMap_HeightBase) * 4;
    AreaMap_Header[row + U(static_cast<short>(x))] = static_cast<unsigned char>(value);
}

// original 0x57C160: bit (index & 7) of bits[(index & 0xFF) >> 3] toggled -
// Flags_Set's and Flags_Clear's xor sibling (the PSX Flag_Toggle 0x8015BFE4,
// the sibling's name, confirmed there; pairs_propagated "call"). eax is left
// the byte's address; no caller reads it.
extern "C" void __cdecl Flags_Toggle(unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFF;
    bits[i >> 3] = static_cast<unsigned char>(bits[i >> 3] ^ (1u << (i & 7)));
}

// original 0x57C5A0: Camera_Angles[0] turned a step (the second argument's
// low byte, s8, 4096-step units) toward the angle (the first argument's low
// word); al 1 while short of it (compared as s16 against the word & 0xFFF);
// else the word the angle and Light_AnglesCopy[0] = Light_Angles[0] + the
// angle + 0x2AA, al 0. Camera_TurnToDegrees' one callee. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_TurnStep(unsigned angle, unsigned step) {
    return TurnStep(at::kAngle0, at::kLightAngle0, at::kLightCopy0, 0x2AA, angle, step);
}

// original 0x57C600: Camera_TurnStepFB(the angle (s16 degrees) x 4096 / 360 &
// 0xFFF, the step (s8 degrees) x 4096 / 360) - each truncated (the imul
// 0xB60B60B7 / add / sar 8 / add sign idiom), the step passed as the whole
// dword the callee reads the low byte of (a step above 11 degrees wraps as
// s8 there). al: Camera_TurnStepFB's (1 while turning). Camera_TurnToDegrees'
// twin; SC2's Scena02_Scene10 tests it twice. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_TurnFBToDegrees(unsigned angle, unsigned step) {
    const std::int32_t s = (static_cast<std::int32_t>(static_cast<signed char>(step)) << 12) / 360;
    const std::int32_t a = ((static_cast<std::int32_t>(static_cast<short>(angle)) << 12) / 360) & 0xFFF;
    return SH_CALL(Camera_TurnStepFB)(U(a), U(s));
}

// original 0x57C650: Camera_TurnStep for Cond_AngleFB (Camera_Angles[2]):
// the step, the test, and at the end the word the angle and
// Light_AnglesCopy[2] = Light_Angles[2] (0x90359C) + the angle - 0x200. al
// 1 while turning. Camera_TurnFBToDegrees' one callee. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_TurnStepFB(unsigned angle, unsigned step) {
    return TurnStep(at::kAngleFB, at::kLightAngle2, at::kLightCopy2, -0x200, angle, step);
}

// original 0x57C8A0: party member m's field object (ObjTrio + m * 0x14C, m
// the first argument's low byte, unchecked): state +1 2, sub-state +2 8, +3 0,
// +0xB the second argument's low byte - Field_TileTurn's state 2 / 8 for a
// member by index. Area 51 passes 1; areas 148, EffectKind28_PushParty 0x4703F0,
// EffectKind2A_PushParty 0x4712E0 and 0x4849A0 pass 2 or a table byte. PSX 0x8015CC04
// (pairs_propagated "call").
extern "C" void __cdecl Member_SetState2_8(unsigned member, unsigned value) {
    unsigned char* const o = ObjTrio + (member & 0xFF) * at::kObjStride;
    o[1] = 2;
    o[2] = 8;
    o[3] = 0;
    o[0xB] = static_cast<unsigned char>(value);
}

// original 0x587860: each of the 23 Sound_Channels dwords (to 0x6BC924) not 0:
// SndBuf_Stop(it), then the dword 0 (after the call). Sound_PauseAll 0x587C30
// walks the same dwords but keeps them (and stops the stream and the music).
// The loss screen's BattleLoss_Restart 0x432750 (BE1's), chapter 15's
// Scena15_Run5 and chapter 17's Scena17_Outro11 / Scena17_EndRestart call it
// before Task_Restart(Boot_Task).
// PSX 0x8015D8AC (pairs_propagated "call").
extern "C" void __cdecl Sound_StopChannels(void) {
    for (std::uint32_t p = Addr(Sound_Channels); p < at::kChannelsEnd; p += 4) {
        const std::uint32_t buffer = U(Long(At(p)));
        if (buffer == 0) continue;
        SH_CALL(SndBuf_Stop)(At(buffer));
        SetLong(At(p), 0);
    }
}

// original 0x587890: the volume of a cue's voices. The cue word (the first
// argument & 0xFFFF): bank ((cue >> 8) & 0xF) - 1 (bank 0 gives -1, a record
// before Sound_Banks: unchecked), its 16-byte cue (cue & 0xFF) at Sound_Banks
// + bank * 0x384 + cue * 16 - four dwords, each read again; for each not 0,
// 0x5A6C60(the buffer of voice (dword & 0xFF), the bank's +0x184 + voice * 8,
// the level dword whole) - a DirectSound SetVolume. As Sound_PlayEffect reads
// the same cue. Chapter 13's Scena13_ToneLevels is the one caller. No PSX
// twin paired.
extern "C" void __cdecl Sound_SetCueVolume(unsigned cue, unsigned level) {
    const std::uint32_t c = cue & 0xFFFF;
    const std::uint32_t bank = (((c >> 8) & 0xF) - 1) * at::kBankStride;
    const std::uint32_t cue_at = Addr(Sound_Banks) + bank + (c & 0xFF) * 16;
    for (unsigned k = 0; k < 4; ++k) {
        const std::uint32_t voice = U(Long(At(cue_at + 4 * k)));
        if (voice == 0) continue;
        const std::uint32_t buffer = U(Long(At(Addr(Sound_Banks) + at::kBankVoices + bank + (voice & 0xFF) * 8)));
        SH_AT(void (__cdecl*)(std::uint32_t, std::uint32_t), at::kSndBufVolume)(buffer, level);
    }
}

// original 0x591920: the first of the 32 key-item bytes (0x904554) equal to
// the argument's low byte cleared, al 1; none, al 0 (the rest of eax the
// list's address bits either way). An item of 0 clears the first empty slot
// and answers 1. KeyItem_Add's opposite; chapter 13's Scena13_Run3 takes
// item 0xB. PSX 0x80166B54 (pairs_propagated "gap9").
extern "C" unsigned char __cdecl KeyItem_Remove(unsigned item) {
    unsigned char* const list = At(at::kKeyItems);
    const auto id = static_cast<unsigned char>(item);
    for (unsigned i = 0; i < 0x20; ++i) {
        if (list[i] != id) continue;
        list[i] = 0;
        return 1;
    }
    return 0;
}

// original 0x591EC0: the ability list an id files in (PSX AbilityList_ForType
// 0x80167514, the sibling's name, confirmed there; pairs_propagated "call").
// With the third argument's low byte 0, CharacterRecords[member byte] (the
// saved record, by the record's own index); else the member's working record
// ObjTrio[member byte] +0x80. The id's type byte (0x65C4D9 + id * 24, & 3: 1
// +0x6A, 2 +0x74, 3 +0x7E, 0 +0x60) picks the 10-byte list. Every index
// unchecked. AbilityList_Add's one callee. Char_AbilityList 0x591E50 is its
// sibling by a member's list slot and a type.
extern "C" unsigned char* __cdecl AbilityList_ForType(unsigned member, unsigned id, unsigned working) {
    static const unsigned char kLists[4] = {0x60, 0x6A, 0x74, 0x7E};
    const unsigned m = member & 0xFF;
    unsigned char* const base = (working & 0xFF) != 0 ? At(at::kWorkingRecords + m * at::kObjStride)
                                                      : At(bof3::addr::CharacterRecords + m * at::kRecordStride);
    const unsigned type = At(at::kAbilityType)[(id & 0xFF) * 24] & 3;
    return base + kLists[type];
}

// original 0x5A7730: a primitive's code byte +7 0x7C (the PSX GPU's SPRT_16)
// and the float 0.01 to +0x10 - Gpu_SetSprt / Gpu_SetSprt8's shape. eax is
// left the primitive; no caller reads it. The PSX SetSprt16 0x8017B380 (the
// sibling's Psy-Q signature match; pairs_propagated "call-anchored").
extern "C" void __cdecl Gpu_SetSprt16(unsigned char* prim) {
    prim[7] = 0x7C;
    SetLong(prim + 0x10, static_cast<std::int32_t>(at::kDepth));
}

void ScenaSx2_Inject() {
    if (bof3::WantsShadow("scena_sx2")) scena_sx2::SelfTest();
    BOF3_INJECT(Effect_HoldFlag1C);
    BOF3_INJECT(Party_PlaceInFormation);
    BOF3_INJECT(AreaMap_SetHeight);
    BOF3_INJECT(Flags_Toggle);
    BOF3_INJECT(Camera_TurnStep);
    BOF3_INJECT(Camera_TurnFBToDegrees);
    BOF3_INJECT(Camera_TurnStepFB);
    BOF3_INJECT(Member_SetState2_8);
    BOF3_INJECT(Sound_StopChannels);
    BOF3_INJECT(Sound_SetCueVolume);
    BOF3_INJECT(KeyItem_Remove);
    BOF3_INJECT(AbilityList_ForType);
    BOF3_INJECT(Gpu_SetSprt16);
}
