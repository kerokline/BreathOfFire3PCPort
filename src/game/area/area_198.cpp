// Moved here by tools/move_funcs.py from area_w4f.cpp (docs/source-layout-round.md).
#include "game/area/area_198.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
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

// Sprite_Objects record `k` (a byte index, unchecked as the originals').
unsigned char* ObjectAt(unsigned k) { return Mem(::bof3::Img(Sprite_Objects_addr) + k * at::kObjectStride); }

// The movement script's position (MoveScript_Object's word +0xA) moved by
// `v`, MoveScript_Object read at the store.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

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

using ObjectFn = void (__cdecl*)(unsigned char*);

using SlotFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*);

// 0x454A80(object): the object's Field_Slots scripts released.
void ReleaseSlots(unsigned char* object) { AH_AT(ObjectFn, at::kSlotsReleaseFor.v)(object); }

// 0x455290(object, script): a Field_Slots script started (its answer unread
// here).
void StartSlot(unsigned char* object, U script) { AH_AT(SlotFn, at::kSlotStart.v)(object, Mem(script)); }

// 0x441090(value, sign): the high word of a 16.16 value, rounded up when sign
// is not negative (ax).
unsigned short RoundHigh(std::int32_t value, std::int32_t sign) {
    return AH_AT(unsigned short (__cdecl*)(std::int32_t, std::int32_t), at::kRoundHigh.v)(value, sign);
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

// Area 198's effect kind 0xA6's record `k` (8 bytes: s16 x scale, s16 z scale,
// u16 animation bank, u8 animation), by the running record's +6 (unchecked).
unsigned char* A6Record(unsigned k) { return Mem(::bof3::Img(Area198_EffectA6Records_addr) + k * 8u); }

}  // namespace

// original 0x42CF00 (area 198 +0x3C[0]; PSX 0x801F2E00): the running
// object's Field_Slots scripts released (0x454A80), Area198_SlotScript0
// started for it (0x455290), its +0x2A 1, Sprite_SetAnimation(0) - Sprite_Current
// read for each. Area 85's 0x40F960 without the +0x2A store.
extern "C" void __cdecl Area198_StartSlotScript0(void) {
    ReleaseSlots(Sprite_Current);
    StartSlot(Sprite_Current, ::bof3::Img(Area198_SlotScript0_addr));
    Sprite_Current[0x2A] = 1;
    AH_CALL(Sprite_SetAnimation)(0);
}

// original 0x42CF40 (area 198 +0x3C[4]; PSX 0x801F2FF8): the running object's
// word +0x3E - 0x10. The script object's byte +2 below 0x80 with bits 0..1
// clear: Sprite_SetAnimationAt(Area198_SinkAnims[bits 2..3], the object's word
// +0x58 - 2) and the script object read again. The script object's +2 + 1.
// The word +0x3E below -0x280 (signed): each of +0x5D, +0x5E, +0x5F - 1 unless
// it is 0x80. The word exactly 0xF600: counter 0 + 1. The word above -0xC80
// (signed): the script position - 2; else the script object's +2 0.
extern "C" void __cdecl Area198_SinkFade(void) {
    AddWord(Sprite_Current + 0x3E, -0x10);
    unsigned char* object = MoveScript_Object;
    const unsigned char v = object[2];
    if (v < 0x80 && (v & 3) == 0) {
        unsigned char* const c = Sprite_Current;
        const unsigned anim = B(::bof3::Img(Area198_SinkAnims_addr) + ((v >> 2) & 3u));
        AH_CALL(Sprite_SetAnimationAt)(static_cast<unsigned char>(anim), static_cast<unsigned short>(Word(c + 0x58) - 2));
        object = MoveScript_Object;
    }
    object[2] = static_cast<unsigned char>(object[2] + 1);
    unsigned char* c = Sprite_Current;
    if (static_cast<std::int16_t>(rv::As<records::SpriteObject>(c)->height_whole) < -0x280) {
        for (unsigned off = 0x5D; off <= 0x5F; ++off) {
            if (c[off] == 0x80) continue;
            c[off] = static_cast<unsigned char>(c[off] - 1);
            c = Sprite_Current;
        }
    }
    if (static_cast<std::uint16_t>(rv::As<records::SpriteObject>(c)->height_whole) == 0xF600) B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter0.v)) + 1);
    if (static_cast<std::int16_t>(rv::As<records::SpriteObject>(c)->height_whole) > static_cast<std::int16_t>(0xF380)) {
        ScriptStep(-2);
        return;
    }
    MoveScript_Object[2] = 0;
}

// original 0x42D000 (area 198 +0x3C[5]; PSX 0x801F3198): three effects of
// kind 0xA6 in turn (Effect_FindFree; none: that one skipped): +0 1, +5 0xA6,
// +6 1, 0 and 2, +7 the active member's index in Sprite_Objects (read after
// each search; a signed quotient, as a byte).
extern "C" void __cdecl Area198_SpawnEffectsA6(void) {
    static const unsigned char kRecords[3] = {1, 0, 2};
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot == 0xFF) continue;
        const unsigned char who = MemberIndex();
        unsigned char* const r = EffectAt(slot);
        r[0] = 1;
        r[5] = 0xA6;
        r[6] = kRecords[k];
        r[7] = who;
    }
}

// original 0x42D100 (area 198 +0x3C[6]; PSX 0x801F3384): x += step << 11,
// z -= step << 11, the word +0x3E += step << 4 (Area198_ShakeSteps by the
// running object's +0xA low nibble, s8, read again for each), +0xA - 1, the
// script position - 2. Counter 1 not 0: an effect of kind 0xAC (+0 1, +5
// 0xAC; none: no effect), counter 1 0, the object's +0x5F, +0x5E, +0x5D 0xC0.
// Counter 1 0: each of +0x5D, +0x5E, +0x5F + 8 unless it is 0.
extern "C" void __cdecl Area198_Shake(void) {
    ShakeXZ(::bof3::Img(Area198_ShakeSteps_addr));
    {
        unsigned char* const c = Sprite_Current;
        const int s = ShakeStep(::bof3::Img(Area198_ShakeSteps_addr));
        rv::Store<unsigned>(rv::As<records::SpriteObject>(c)->height_whole, static_cast<unsigned>(rv::As<records::SpriteObject>(c)->height_whole + (static_cast<unsigned>(s) << 4)));
    }
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    ScriptStep(-2);
    if (B(::bof3::Img(at::kCounter1.v)) != 0) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            EffectAt(slot)[0] = 1;
            EffectAt(slot)[5] = 0xAC;
        }
        unsigned char* const c = Sprite_Current;
        B(::bof3::Img(at::kCounter1.v)) = 0;
        c[0x5F] = 0xC0;
        Sprite_Current[0x5E] = 0xC0;
        Sprite_Current[0x5D] = 0xC0;
        return;
    }
    unsigned char* c = Sprite_Current;
    for (unsigned off = 0x5D; off <= 0x5F; ++off) {
        if (c[off] == 0) continue;
        c[off] = static_cast<unsigned char>(c[off] + 8);
        c = Sprite_Current;
    }
}

// original 0x42D200 (area 198 +0x3C[7]; PSX 0x801F3524): d = (0x190000 - the
// running object's x) >> 15 (32-bit, arithmetic); not 0: the script object's
// +7 = abs(d) as a byte, the running object's +8 (its direction) 3,
// MoveCmd_Move(the script object, +8) - each pointer read again.
extern "C" void __cdecl Area198_WalkToX190(void) {
    const auto d = static_cast<std::int32_t>(0x190000u - static_cast<U>(rv::Current<records::SpriteObject>()->x)) >> 15;
    if (d == 0) return;
    MoveScript_Object[7] = static_cast<unsigned char>(d < 0 ? 0u - static_cast<U>(d) : static_cast<U>(d));
    Sprite_Current[8] = 3;
    unsigned char* const c = Sprite_Current;
    unsigned char* const object = MoveScript_Object;
    AH_CALL(MoveCmd_Move)(object, c[8]);
}

// original 0x42D250 (area 198 +0x3C[8], PSX 0x801F35A0; also area 174
// +0x3C[1]): Field_Request 5: the running object's Field_Slots scripts
// released (0x454A80). Else the script position - 2 (it waits).
extern "C" void __cdecl Area198_ReleaseOnRequest5(void) {
    if (Field_Request == 5) {
        ReleaseSlots(Sprite_Current);
        return;
    }
    ScriptStep(-2);
}

// original 0x42D280 (area 198 +0x3C[9]; PSX 0x801F35FC): every eighth frame
// (Frame_Counter's low 3 bits 0) with counter 0 above 2: the running object's
// +0xB with bits 0..2 clear - Sound_PlayEffect(0) (the original computes the
// id as `((b & 8) + 0x203) == 0` through neg / sbb / inc, which is 0 for every
// b); Area198_SpawnDrop, and again with counter 0 (read again) above 3. The
// script position - 2 every time.
extern "C" void __cdecl Area198_SpawnDrops(void) {
    if ((Frame_Counter & 7) == 0 && B(::bof3::Img(at::kCounter0.v)) > 2) {
        if ((Sprite_Current[0xB] & 7) == 0) AH_CALL(Sound_PlayEffect)(0);
        AH_CALL(Area198_SpawnDrop)();
        if (B(::bof3::Img(at::kCounter0.v)) > 3) AH_CALL(Area198_SpawnDrop)();
    }
    ScriptStep(-2);
}

// original 0x42D2E0 (called by Area198_SpawnDrops): Sprite_FindFree into the
// scratch word 0x903850; a slot: EventOp_0x(Area198_DropOp), then the new
// Sprite_Current's dwords +0x10, +0xC 0 and +0x14 8, +9 0x10, the word +0x3E
// (Rand() % 30 - 6) << 5, +0x2B 1, and a cell (a, b) drawn as a = Rand() % 14
// + 0x10, b = Rand() % 25 + 0xB (bytes; the CRT's remainders, signed) again
// while a is below 0x1A and b below 0x15: x = a << 16, z = b << 16
// (Sprite_Current read for each store). Then Sprite_Current put back as it was
// on entry and its +0xB + 1.
extern "C" void __cdecl Area198_SpawnDrop(void) {
    unsigned char* const self = Sprite_Current;
    const unsigned char slot = AH_CALL(Sprite_FindFree)();
    SetWord(Mem(::bof3::Img(at::kObjectIndex.v)), slot);
    if (slot != 0xFF) {
        AH_CALL(EventOp_0x)(Mem(::bof3::Img(Area198_DropOp_addr)));
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, 0);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, 0);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_y, 8);
        Sprite_Current[9] = 0x10;
        const int r = AH_CALL(Rand)();
        rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>((r % 30 - 6) << 5));
        Sprite_Current[0x2B] = 1;
        unsigned char a, b;
        do {
            a = static_cast<unsigned char>(AH_CALL(Rand)() % 14 + 0x10);
            b = static_cast<unsigned char>(AH_CALL(Rand)() % 25 + 0xB);
        } while (static_cast<int>(a) - 0x15 < 5 && static_cast<int>(b) - 0x10 < 5);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(static_cast<U>(a) << 16));
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(static_cast<U>(b) << 16));
    }
    Sprite_Current = self;
    self[0xB] = static_cast<unsigned char>(self[0xB] + 1);
}

// original 0x42D3F0 (area 198 +0x3C[10]; PSX 0x801F3850): an effect of kind
// 0xA7 (Effect_FindFree's answer kept at the running object's +0xB and read
// back from there for each store): none - the script position - 2 (it waits);
// else +0 1, +5 0xA7, and the record's x, z, y the object's (y + 0x1000000).
extern "C" void __cdecl Area198_SpawnEffectA7(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const c = Sprite_Current;
    auto* const tC = rv::As<records::SpriteObject>(c);
    if (c[0xB] == 0xFF) {
        ScriptStep(-2);
        return;
    }
    EffectAt(c[0xB])[0] = 1;
    EffectAt(c[0xB])[5] = 0xA7;
    unsigned char* const r34 = EffectAt(c[0xB]);
    auto* const tR34 = rv::As<records::EffectObject>(r34);
    tR34->x = tC->x;
    const std::int32_t z = tC->z;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(c[0xB]))->z, z);
    const std::int32_t y = tC->height;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(c[0xB]))->height, static_cast<std::int32_t>(static_cast<U>(y) + 0x1000000u));
}

// original 0x42D470 (area 198 +0x3C[11]; PSX 0x801F39B0): the running
// object's Field_Slots scripts released, Area198_SlotScript11 started for it,
// its +0x2A 1, its +0 bit 5, Sprite_SetAnimation(0) - Sprite_Current read for
// each.
extern "C" void __cdecl Area198_StartSlotScript11(void) {
    ReleaseSlots(Sprite_Current);
    StartSlot(Sprite_Current, ::bof3::Img(Area198_SlotScript11_addr));
    Sprite_Current[0x2A] = 1;
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    AH_CALL(Sprite_SetAnimation)(0);
}

// original 0x42D4B0 (Effect_KindHandlers[0xA6], .data 0x6555E8; the kind area
// 198's handler 5 spawns): Area198_EffectA6States by the running record's +1
// (unchecked there; ours aborts past its two).
extern "C" void __cdecl Area198_EffectA6Run(void) {
    StateEntry("Area198_EffectA6Run", ::bof3::Img(Area198_EffectA6States_addr), Sprite_Current[1])();
}

// original 0x42D4D0 (Area198_EffectA6States[0]): the record's x, z, y field
// object +7's (Sprite_Objects, the byte unchecked; Sprite_Current and +7 read
// again for each); Sprite_SetAnimationBank(the bank of record +6); +0x48 1,
// +0x24 0, +0x2A 0; Sprite_SetAnimation(the animation of record +6); +1 1;
// then a tail jmp into state 1 (0x42D580, the next byte).
extern "C" void __cdecl Area198_EffectA6Start(void) {
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, rv::As<records::SpriteObject>(ObjectAt(Sprite_Current[7]))->x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, rv::As<records::SpriteObject>(ObjectAt(Sprite_Current[7]))->z);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, rv::As<records::SpriteObject>(ObjectAt(Sprite_Current[7]))->height);
    AH_CALL(Sprite_SetAnimationBank)(Word(A6Record(Sprite_Current[6]) + 4));
    Sprite_Current[0x48] = 1;
    Sprite_Current[0x24] = 0;
    Sprite_Current[0x2A] = 0;
    AH_CALL(Sprite_SetAnimation)(A6Record(Sprite_Current[6])[6]);
    Sprite_Current[1] = 1;
    AH_CALL(Area198_EffectA6Follow)();
}

// original 0x42D580 (Area198_EffectA6States[1]; also reached by state 0's
// tail jmp): o = field object +7 (read once); the record's +0 o's +0;
// Sprite_ScriptTick, Sprite_QueueOverlay; then o's +0x5C..+0x5F, +0x27, +0x48,
// dwords +0x40, +0x44, words +0x2E, +0x30, +0x32 and dword +0x60 copied to the
// record (Sprite_Current read for each), +0x29 6. The record's +0x48 (read
// back) not 0: its words +0x2E / +0x30 += 0x441090(record +6's s16 x / z
// scale * the dword +0x40 / +0x44, that dword); 0: += 0x441090(scale << 16,
// 0x10000). Each word's address taken before its call, added to after.
extern "C" void __cdecl Area198_EffectA6Follow(void) {
    unsigned char* const o = ObjectAt(Sprite_Current[7]);
    auto* const tO = rv::As<records::SpriteObject>(o);
    Sprite_Current[0] = o[0];
    AH_CALL(Sprite_ScriptTick)();
    AH_CALL(Sprite_QueueOverlay)();
    Sprite_Current[0x5C] = o[0x5C];
    Sprite_Current[0x5D] = o[0x5D];
    Sprite_Current[0x5E] = o[0x5E];
    Sprite_Current[0x5F] = o[0x5F];
    Sprite_Current[0x27] = o[0x27];
    Sprite_Current[0x48] = o[0x48];
    rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->scale_x, tO->scale_x);
    rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->scale_y, tO->scale_y);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_x, static_cast<std::uint16_t>(tO->screen_x));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_y, static_cast<std::uint16_t>(tO->screen_y));
    rv::Store<unsigned>(rv::Current<records::EffectObject>()->draw_key, tO->draw_key);
    rv::Store<std::int32_t>(rv::Current<records::EffectObject>()->depth, tO->depth);
    Sprite_Current[0x29] = 6;
    unsigned char* c = Sprite_Current;
    if (c[0x48] != 0) {
        std::int32_t scale = rv::As<records::EffectObject>(c)->scale_x;
        unsigned char* w = c + 0x2E;
        std::int32_t v = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6])))) * static_cast<U>(scale));
        unsigned short r = RoundHigh(v, scale);
        SetWord(w, static_cast<unsigned>(Word(w) + r));
        c = Sprite_Current;
        scale = rv::As<records::EffectObject>(c)->scale_y;
        w = c + 0x30;
        v = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6]) + 2))) * static_cast<U>(scale));
        r = RoundHigh(v, scale);
        SetWord(w, static_cast<unsigned>(Word(w) + r));
        return;
    }
    unsigned char* w = c + 0x2E;
    unsigned short r = RoundHigh(static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6])))) << 16), 0x10000);
    SetWord(w, static_cast<unsigned>(Word(w) + r));
    c = Sprite_Current;
    w = c + 0x30;
    r = RoundHigh(static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6]) + 2))) << 16), 0x10000);
    SetWord(w, static_cast<unsigned>(Word(w) + r));
}

void Area198_Inject() {
    BOF3_INJECT(Area198_StartSlotScript0);
    BOF3_INJECT(Area198_SinkFade);
    BOF3_INJECT(Area198_SpawnEffectsA6);
    BOF3_INJECT(Area198_Shake);
    BOF3_INJECT(Area198_WalkToX190);
    BOF3_INJECT(Area198_ReleaseOnRequest5);
    BOF3_INJECT(Area198_SpawnDrops);
    BOF3_INJECT(Area198_SpawnDrop);
    BOF3_INJECT(Area198_SpawnEffectA7);
    BOF3_INJECT(Area198_StartSlotScript11);
    BOF3_INJECT(Area198_EffectA6Run);
    BOF3_INJECT(Area198_EffectA6Start);
    BOF3_INJECT(Area198_EffectA6Follow);
}
