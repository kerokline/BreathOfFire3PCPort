// Moved here by tools/move_funcs.py from area_w3c.cpp, area_w3d.cpp (docs/source-layout-round.md).
#include "game/area/area_134.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area/area_134_callees.h"
#include "game/area_harness.h"
#include "game/area_w3c.h"
#include "game/area_w3c_callees.h"
#include "game/area_w3d.h"
#include "game/area_w3d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_134::at;

}  // namespace

namespace {

using area_harness::Mem;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

// Effect_Spawn(kind, 0, args[party list `member`'s byte], party record
// `member`'s words +0x2E and +0x30) with Sprite_Current made that record (and
// left so); an answer other than 0xFF stored to Sprite_Current[0xB], read
// again after the call. The list byte indexes the table unchecked (it stays in
// .data). Area 94's handlers' shape (area_w2c.cpp's SpawnAt).
void SpawnAtMember(unsigned member, unsigned char kind, U args) {
    const unsigned char* const record = PartyAt(member);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_x));
    const unsigned char effect = B(args + B(::bof3::Img(Cond_Flags_addr) + 0xd2 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// An effect record of `kind` at the running object (areas 130 and 134): a slot
// from Effect_FindFree, none (0xFF) nothing; else its +0 = 1, +5 = kind, +1 =
// 0 when `clear1`, and its x, z, y dwords (+0x34, +0x38, +0x3C) the running
// object's, Sprite_Current read again for each.
void EffectAtObject(unsigned char kind, bool clear1) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = kind;
    if (clear1) e[1] = 0;
    tE->x = rv::Current<records::SpriteHead>()->x;
    tE->z = rv::Current<records::SpriteHead>()->z;
    tE->height = rv::Current<records::SpriteHead>()->height;
}

using move_script::SetWord;

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Effect record `slot` (Effect_Objects, 20 of 0x80 bytes). Choice 2 stores
// Effect_FindFree's "none", 0xFF, as the slot the tail later writes through,
// unchecked: past the 20 records ours aborts.
unsigned char* EffectAtAreaW3d(const char* who, unsigned slot) {
    if (slot >= at::kEffectCount)
        bof3::Fatal("%s: the effect slot %u is past the 20 effect records (the original writes 0x%X, past "
                    "Effect_Objects)",
                    who, slot, static_cast<unsigned>(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride));
    return Mem(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride);
}

}  // namespace

// original 0x41D790 (+0x3C[0], PSX 0x801F5114): kind 4 at record 0, args B.
extern "C" void __cdecl Area134_SpawnKind4AtMember0(void) { SpawnAtMember(0, 4, ::bof3::Img(Area134_EffectArgsB_addr)); }

// original 0x41D7E0 (+0x3C[1], PSX 0x801F5194): Camera_ShiftX + 2, redraw.
extern "C" void __cdecl Area134_CameraShiftXMore2(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX + 2);
    MapView_Redraw = 2;
}

// original 0x41D800 (+0x3C[2], PSX 0x801F51BC): Camera_ShiftX - 2, redraw.
extern "C" void __cdecl Area134_CameraShiftXLess2(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX - 2);
    MapView_Redraw = 2;
}

// original 0x41D810 (+0x3C[3], PSX 0x801F51E4; also area 8's choice 7 =
// handler 6): Camera_ShiftX 0, redraw.
extern "C" void __cdecl Area134_CameraShiftXReset(void) {
    Camera_ShiftX = 0;
    MapView_Redraw = 2;
}

// original 0x41D830 (+0x3C[4], PSX 0x801F5200): kind 1 at record 0, args A.
extern "C" void __cdecl Area134_SpawnKind1AtMember0(void) { SpawnAtMember(0, 1, ::bof3::Img(Area134_EffectArgsA_addr)); }

// original 0x41D880 (+0x3C[5], PSX 0x801F5280): Camera_ShiftY + 8, redraw.
extern "C" void __cdecl Area134_CameraShiftYMore8(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 8);
    MapView_Redraw = 2;
}

// original 0x41D890 (+0x3C[6], PSX 0x801F52A8): Camera_ShiftY - 8, redraw.
extern "C" void __cdecl Area134_CameraShiftYLess8(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 8);
    MapView_Redraw = 2;
}

// original 0x41D8A0 (+0x3C[7], PSX 0x801F52D0): kind 3 at record 1, args A.
extern "C" void __cdecl Area134_SpawnKind3AtMember1(void) { SpawnAtMember(1, 3, ::bof3::Img(Area134_EffectArgsA_addr)); }

// original 0x41D8F0 (+0x3C[8], PSX 0x801F5350): kind 3 at record 2, args A.
extern "C" void __cdecl Area134_SpawnKind3AtMember2(void) { SpawnAtMember(2, 3, ::bof3::Img(Area134_EffectArgsA_addr)); }

// original 0x41D940 (+0x3C[10], PSX 0x801F53EC): kind 1 at record 1, args A.
extern "C" void __cdecl Area134_SpawnKind1AtMember1(void) { SpawnAtMember(1, 1, ::bof3::Img(Area134_EffectArgsA_addr)); }

// original 0x41D990 (+0x3C[11], PSX 0x801F546C): kind 1 at record 2, args A.
extern "C" void __cdecl Area134_SpawnKind1AtMember2(void) { SpawnAtMember(2, 1, ::bof3::Img(Area134_EffectArgsA_addr)); }

// original 0x41D9E0 (+0x3C[12] and [13], PSX 0x801F54EC and 0x801F557C): an
// effect record of kind 0x90 at the running object, +1 = 0.
extern "C" void __cdecl Area134_Effect90AtObject(void) { EffectAtObject(0x90, true); }

// original 0x41DA30 (+0x3C[14], PSX 0x801F560C): kind 0x93 at the running
// object.
extern "C" void __cdecl Area134_Effect93AtObject(void) { EffectAtObject(0x93, false); }

// original 0x41DA80 (+0x3C[15], PSX 0x801F5698): kind 0x99 at the running
// object.
extern "C" void __cdecl Area134_Effect99AtObject(void) { EffectAtObject(0x99, false); }

// original 0x41DAD0 (Area135_Choices[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, tail kind 0x12 at state 0.
extern "C" void __cdecl Area135_ChoiceStartTail(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x12;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x41DB00 (Area135_Choices[1]): message 0x84; the tail's sub byte =
// the answer + 1, printed into Text_Records by area 8's format
// (Crt_sprintf); the marker's +0x83 = the answer (read again) + 4.
extern "C" void __cdecl Area135_ChoiceCount(void) {
    const auto count = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) + 1);
    SetMessage(0x84);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = count;
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(Mem(::bof3::Img(at::kTextRecords.v))),
                         reinterpret_cast<const char*>(Mem(::bof3::Img(at::kMessageFormat.v))), static_cast<unsigned>(count));
    B(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x26f) = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) + 4);
}

// original 0x41DB40 (Area135_Choices[2]): the s8 answer 0 or 1: ScriptFlags_Set40,
// tail kind 0x12 at state 0x14, Effect_FindFree to the slot byte 0x675CC0 (the
// "none" 0xFF stored too); a slot: +0 = 1, kind +5 = 0x66, +6 = the answer.
// Any answer: message 0xFFFF.
extern "C" void __cdecl Area135_ChoiceStartTail14(void) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    if (answer == 0 || answer == 1) {
        const auto which = static_cast<unsigned char>(answer);
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x12;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        B(::bof3::Img(AreaW3d_EffectSlot_addr)) = slot;
        if (slot != 0xFF) {
            unsigned char* const e = EffectAtAreaW3d("Area135_ChoiceStartTail14", B(::bof3::Img(AreaW3d_EffectSlot_addr)));
            e[0] = 1;
            e[5] = 0x66;
            e[6] = which;
        }
    }
    SetMessage(0xFFFF);
}

// original 0x41DBB0 (Area135_Choices[3]): message 0xFFFF; answer 0 sets story
// flag 0x37, any other clears it.
extern "C" void __cdecl Area135_ChoiceFlag37(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer == 0)
        AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x37);
    else
        AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x37);
}

void Area134_Inject() {
    BOF3_INJECT(Area134_SpawnKind4AtMember0);
    BOF3_INJECT(Area134_CameraShiftXMore2);
    BOF3_INJECT(Area134_CameraShiftXLess2);
    BOF3_INJECT(Area134_CameraShiftXReset);
    BOF3_INJECT(Area134_SpawnKind1AtMember0);
    BOF3_INJECT(Area134_CameraShiftYMore8);
    BOF3_INJECT(Area134_CameraShiftYLess8);
    BOF3_INJECT(Area134_SpawnKind3AtMember1);
    BOF3_INJECT(Area134_SpawnKind3AtMember2);
    BOF3_INJECT(Area134_SpawnKind1AtMember1);
    BOF3_INJECT(Area134_SpawnKind1AtMember2);
    BOF3_INJECT(Area134_Effect90AtObject);
    BOF3_INJECT(Area134_Effect93AtObject);
    BOF3_INJECT(Area134_Effect99AtObject);
    BOF3_INJECT(Area135_ChoiceStartTail);
    BOF3_INJECT(Area135_ChoiceCount);
    BOF3_INJECT(Area135_ChoiceStartTail14);
    BOF3_INJECT(Area135_ChoiceFlag37);
}
