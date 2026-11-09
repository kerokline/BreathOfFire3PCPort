// Moved here by tools/move_funcs.py from area_w4f.cpp (docs/source-layout-round.md).
#include "game/area/area_197.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4f::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

U Key(const void* p) { return Low32(p); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned index) {
    if (index >= at::kStateCount)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, at::kStateCount, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The signed quotient (Field_ActiveMember - Sprite_Objects) / 0xA4 the
// effect spawns keep (the originals' multiply by 0x63E7063F, sar 6, plus the
// sign bit: truncating toward 0).
unsigned char MemberIndex() {
    const auto d = static_cast<std::int32_t>(Key(Field_ActiveMember) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(d / static_cast<std::int32_t>(at::kObjectStride));
}

// The member search of areas 196 and 197's handlers (nine bodies, one a
// handler, each over its own two tables; AR3F's Area143_MessageByMember is
// the same code): for each of the four keys in turn, each of
// Field_MemberCount's party records (the count read once, compared
// unsigned): the first record whose +0x89 is the key opens the key's message
// (Msg_OpenScript) and sets Field_Request 2. None: nothing. The records are
// not bounded by three (the count is).
void MessageByMember(U keys) {
    const U messages = keys + at::kKeysToMessages;
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char key = B(keys + i);
        for (unsigned char j = 0; j < count; ++j) {
            if (B(::bof3::Img(ObjTrio_addr) + 0x89 + j * at::kPartyStride) != key) continue;
            AH_CALL(Msg_OpenScript)(Word(Mem(messages + i * 2)));
            Field_Request = 2;
            return;
        }
    }
}

// The tail armed after ScriptFlags_Set40: kind `kind`, state `state`.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The run armed after ScriptFlags_Set40: MoveScript_Var7 `var`, its step
// byte `step`.
void ArmRun(signed char var, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = var;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
}

// The s8 step a shake table gives for the running object's +0xA (its low
// nibble), Sprite_Current read at the call.
int ShakeStep(U table) { return static_cast<signed char>(B(table + (Sprite_Current[0xA] & 0xFu))); }

// The +0x34 / +0x38 moves of both shakes: x by step << 11, z by -step << 11
// (32-bit), Sprite_Current and the step read again for each.
void ShakeXZ(U table) {
    unsigned char* c = Sprite_Current;
    auto* tC = rv::As<records::SpriteObject>(c);
    std::int32_t x = tC->x;
    int s = ShakeStep(table);
    tC->x = static_cast<std::int32_t>(static_cast<U>(x) + (static_cast<U>(s) << 11));
    c = Sprite_Current;
    tC = rv::As<records::SpriteObject>(c);
    s = ShakeStep(table);
    tC->z = static_cast<std::int32_t>(static_cast<U>(tC->z) + (static_cast<U>(-s) << 11));
}

}  // namespace

// originals 0x42C8B0, 0x42C940, 0x42C9D0, 0x42CA60, 0x42CAF0, 0x42CB80,
// 0x42CC10 (area 197 +0x3C[0..6]; PSX 0x801F39E0 .. 0x801F3E78):
// MessageByMember over Area197_Keys0..6 (0xC bytes apart).
extern "C" void __cdecl Area197_MessageByMember0(void) { MessageByMember(::bof3::Img(Area197_Keys_addr)); }

extern "C" void __cdecl Area197_MessageByMember1(void) { MessageByMember(::bof3::Img(Area197_Keys_addr) + 1 * at::kKeysStride); }

extern "C" void __cdecl Area197_MessageByMember2(void) { MessageByMember(::bof3::Img(Area197_Keys_addr) + 2 * at::kKeysStride); }

extern "C" void __cdecl Area197_MessageByMember3(void) { MessageByMember(::bof3::Img(Area197_Keys_addr) + 3 * at::kKeysStride); }

extern "C" void __cdecl Area197_MessageByMember4(void) { MessageByMember(::bof3::Img(Area197_Keys_addr) + 4 * at::kKeysStride); }

extern "C" void __cdecl Area197_MessageByMember5(void) { MessageByMember(::bof3::Img(Area197_Keys_addr) + 5 * at::kKeysStride); }

extern "C" void __cdecl Area197_MessageByMember6(void) { MessageByMember(::bof3::Img(Area197_Keys_addr) + 6 * at::kKeysStride); }

// original 0x42CCA0 (area 197 +0x3C[7]; PSX 0x801F3F3C): Area197_ShakeStates
// by the running object's +4 (unchecked there; ours aborts past its two).
extern "C" void __cdecl Area197_RunShake(void) {
    StateEntry("Area197_RunShake", ::bof3::Img(Area197_ShakeStates_addr), Sprite_Current[4])();
}

// original 0x42CCC0 (Area197_ShakeStates[0]): the running object's +0xA 4 and
// +4 1 (Sprite_Current read for each); Field_ActiveMember's word +0x8A - 2.
extern "C" void __cdecl Area197_ShakeStart(void) {
    Sprite_Current[0xA] = 4;
    Sprite_Current[4] = 1;
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x42CCF0 (Area197_ShakeStates[1]): the running object's +0xA 0:
// its +4 0. Else x += step << 11 and z -= step << 11 (Area197_ShakeSteps by
// +0xA's low nibble, s8), +0xA - 1, Field_ActiveMember's word +0x8A - 2.
extern "C" void __cdecl Area197_ShakeStep(void) {
    unsigned char* const c = Sprite_Current;
    if (c[0xA] == 0) {
        c[4] = 0;
        return;
    }
    ShakeXZ(::bof3::Img(Area197_ShakeSteps_addr));
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x42CD60 (area 197 +0x3C[8]; PSX 0x801F4050): an effect of kind
// 0x92 (Effect_FindFree; none: nothing): +0 1, +5 0x92, words +0x2E 0 and
// +0x30 -0x1E, +0x29 6, +0xB the active member's index in Sprite_Objects
// (read after the search; a signed quotient, as a byte).
extern "C" void __cdecl Area197_SpawnEffect92(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    const unsigned char who = MemberIndex();
    unsigned char* const r = EffectAt(slot);
    auto* const tR = rv::As<records::EffectObject>(r);
    r[0] = 1;
    r[5] = 0x92;
    tR->screen_x = 0;
    tR->screen_y = 0xFFE2;
    r[0x29] = 6;
    r[0xB] = who;
}

// original 0x42CDD0 (Field_ModeTailKinds[51]): by the s8 state - 0:
// Party_DropIn(0), state 1; 1: once counter 3 is 0x24: story flag 0x55 set,
// Field_ChangeArea(0xAA, 0x60000, 0x3F0000, 0x82), state 0x1E. Any other state
// nothing.
extern "C" void __cdecl Area197_Tail51(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    if (state == 0) {
        AH_CALL(Party_DropIn)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    }
    if (state != 1 || B(::bof3::Img(at::kCounter3.v)) != 0x24) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x55);
    AH_CALL(Field_ChangeArea)(0xAA, 0x60000, 0x3F0000, 0x82);
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0x1E;
}

// original 0x42CE30 (Area_StepHook's case for area 0xC5): only with
// Cond_ByteFD 4. A: z exactly 0x708000 or 0x738000 with x's high word 3..5;
// or x exactly 0x28000 or 0x58000 with z's high word 0x71..0x73 (the 16-bit
// compares of the high words). B: x exactly 0xD8000 with z's high word 0x71
// or 0x72 and key item 0xD not held. A with key item 0xD held:
// ScriptFlags_Set40, tail kind 0x33 at state 0, al 1; A without it, and B:
// ScriptFlags_Set40, MoveScript_Var7 5, its step 0x14, al 1. Else al 0.
extern "C" unsigned char __cdecl Area197_StepHook(long x, long z) {
    if (Cond_ByteFD != 4) return 0;
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const auto xh = static_cast<std::uint16_t>(ux >> 16), zh = static_cast<std::uint16_t>(uz >> 16);
    bool a = false;
    if ((uz == 0x708000 || uz == 0x738000) && static_cast<std::uint16_t>(xh - 3) < 3) a = true;
    if (!a) {
        if (ux == 0x28000 || ux == 0x58000) {
            if (static_cast<std::uint16_t>(zh - 0x71) >= 3) return 0;
            a = true;
        } else if (ux == 0xD8000) {
            if (static_cast<std::uint16_t>(zh - 0x71) >= 2) return 0;
            if (AH_CALL(KeyItem_Has)(0xD) != 0) return 0;
        } else {
            return 0;
        }
    }
    if (a && AH_CALL(KeyItem_Has)(0xD) != 0) {
        ArmTail(0x33, 0);
        return 1;
    }
    ArmRun(5, 0x14);
    return 1;
}

void Area197_Inject() {
    BOF3_INJECT(Area197_MessageByMember0);
    BOF3_INJECT(Area197_MessageByMember1);
    BOF3_INJECT(Area197_MessageByMember2);
    BOF3_INJECT(Area197_MessageByMember3);
    BOF3_INJECT(Area197_MessageByMember4);
    BOF3_INJECT(Area197_MessageByMember5);
    BOF3_INJECT(Area197_MessageByMember6);
    BOF3_INJECT(Area197_RunShake);
    BOF3_INJECT(Area197_ShakeStart);
    BOF3_INJECT(Area197_ShakeStep);
    BOF3_INJECT(Area197_SpawnEffect92);
    BOF3_INJECT(Area197_Tail51);
    BOF3_INJECT(Area197_StepHook);
}
