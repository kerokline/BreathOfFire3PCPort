// Moved here by tools/move_funcs.py from effect_2d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_46.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_2d.h"
#include "game/effect_2d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + which]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table, or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned which) {
    const unsigned state = Sprite_Current[which];
    if (state >= entries)
        bof3::Fatal("%s: byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2d.md section 2)",
                    who, which, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// Sprite_Objects record `index`: a Fatal past the thirty, where the original
// writes past the pool (Sprite_FindFree answers 0..29 or 0xFF).
unsigned char* SpriteRec(const char* who, unsigned index) {
    if (index >= at::kSprites)
        bof3::Fatal("%s: sprite index %u is past Sprite_Objects' 30 records - the original writes past the pool "
                    "(docs/effect_2d.md section 2)",
                    who, index);
    return rv::Bytes(Sprite_Objects) + index * at::kSpriteStride;
}

// States 1 and 2: +9 down; not 0: each colour byte +0x5D..+0x5F plus `step`;
// 0: +9 = 0xC, +1 = 3. Then the flash and the sprites.
void Fade(unsigned char step) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + step);
        s[0x5E] = static_cast<unsigned char>(s[0x5E] + step);
        s[0x5F] = static_cast<unsigned char>(s[0x5F] + step);
    } else {
        s[9] = 0xC;
        s[1] = 3;
    }
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}

}  // namespace

// original 0x477BA0 (0x12 bytes): jmp [EffectKind46_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind46_Run(void) { Dispatch("EffectKind46_Run", AddressOf(EffectKind46_States), EffectKind46_States_count, 1); }

// original 0x477BC0 (state 0): +9 = 0xC, +1 = 1, +2 = 0, the colour +0x5F /
// +0x5E / +0x5D 0; the flash drawn and the sprites redrawn (a tail jump).
extern "C" void __cdecl EffectKind46_Start(void) {
    S()[9] = 0xC;
    S()[1] = 1;
    S()[2] = 0;
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}

// original 0x477C10 (state 1): the colour up 4 a frame for 11 frames, then state 3.
extern "C" void __cdecl EffectKind46_FadeIn(void) { Fade(4); }

// original 0x477C70 (state 2): the colour down 4 a frame for 11 frames, then state 3.
extern "C" void __cdecl EffectKind46_FadeOut(void) { Fade(0xFC); }

// original 0x477CD0 (state 3): +2 (the count) up; at 5 the counter byte
// 0x903848 up and +1 = 4; else +1 = 1 + (+2 & 1) - a fade in after an even
// count, out after an odd. Then the flash and the sprites.
extern "C" void __cdecl EffectKind46_Count(void) {
    unsigned char* s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
    s = S();
    if (s[2] == 5) {
        At(::bof3::Img(at::kCounter0.v))[0] = static_cast<unsigned char>(At(::bof3::Img(at::kCounter0.v))[0] + 1);
        s[1] = 4;
    } else {
        s[1] = static_cast<unsigned char>((s[2] & 1) + 1);
    }
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}

// original 0x477D10 (state 4): the flash and the sprites, each frame (nothing
// here ends the record).
extern "C" void __cdecl EffectKind46_Hold(void) {
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}

// original 0x477D20: a draw mode at Gfx_PacketNext (Gpu_SetDrawMode(0, 1, 0x4F,
// 0)) committed at slot 5 (Gfx_CommitPrim(5, 0xC)); then a TILE at the new
// Gfx_PacketNext: the colour +0x5D, +0x5E, +0x5F at +4..+6, (0, 0) and 320 x
// 240 as floats, semi-transparent, untextured shading off (Gpu_SetShadeTex 0),
// Gfx_CommitPrim(5, 0x1C).
extern "C" void __cdecl EffectKind46_DrawFlash(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x4F, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    p[4] = S()[0x5D];
    p[5] = S()[0x5E];
    const unsigned char blue = S()[0x5F];
    SetLong(p + 8, std::bit_cast<std::int32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    p[6] = blue;
    SetLong(p + 0xC, 0);
    SetUL(p + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetUL(p + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0x1C);
}

// original 0x477DB0 (a shared tail: the five states jump to it): for each of the
// 30 Sprite_Objects records, Sprite_Current = it and, with +0 bit 0, +0x29 = 5
// and Sprite_UpdateScreen; then Sprite_Current = the leader (ObjTrio), its
// +0x29 = 5, Sprite_UpdateScreen; Sprite_Current put back.
extern "C" void __cdecl EffectKind46_RedrawSprites(void) {
    unsigned char* const saved = Sprite_Current;
    for (unsigned i = 0; i < at::kSprites; ++i) {
        unsigned char* const o = rv::Bytes(&Sprite_Objects[i]);
        Sprite_Current = o;
        if ((o[0] & 1) != 0) {
            o[0x29] = 5;
            SH_CALL(Sprite_UpdateScreen)();
        }
    }
    Sprite_Current = rv::Bytes(ObjTrio);
    ObjTrio[0].ot_slot = 5;
    SH_CALL(Sprite_UpdateScreen)();
    Sprite_Current = saved;
}

// original 0x477E10 (0x12 bytes): jmp [EffectKind47_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind47_Run(void) { Dispatch("EffectKind47_Run", AddressOf(EffectKind47_States), EffectKind47_States_count, 1); }

// original 0x477E30 (state 0): two free sprites into +3 and +4 (none: nothing;
// only the second: the first given back), each marked in use. The model's
// animation byte +0x4B - the leader's, or sprite object 2's when +0xB is 0 -
// looked up among the five triples at 0x654564 (the sixth, past them, when none
// matches). For each sprite: SharedScratch's word = its index,
// EventOp_0x(0x654540 + 17 * +0xB), Sprite_SetAnimationAt(the triple's
// animation, its start). Sprite_Current put back; the first moved (-0x1000,
// +0x1000), the second (+0x1000, -0x1000), both at the leader's height, +0 |=
// 0x20, +0x5C = 0, tinted (0xF, 0, 0, 1) and (0, 0, 0xF, 1); +9 = 4, +1 = 1
// (Sprite_Current read again after the tints). The record is the one read
// after the second Sprite_FindFree throughout.
extern "C" void __cdecl EffectKind47_Start(void) {
    const char* const who = "EffectKind47_Start";
    S()[3] = SH_CALL(Sprite_FindFree)();
    if (S()[3] == 0xFF) return;
    SpriteRec(who, S()[3])[0] = 1;
    S()[4] = SH_CALL(Sprite_FindFree)();
    unsigned char* const self = S();
    if (self[4] == 0xFF) {
        SpriteRec(who, self[3])[0] = 0;
        return;
    }
    SpriteRec(who, self[4])[0] = 1;
    unsigned char model = ObjTrio[0].animation;
    if (self[0xB] == 0) model = Sprite_Objects[2].animation;
    unsigned k = 0;
    while (k < 5 && model != At(::bof3::Img(at::kTwinAnims.v) + 3 * k)[0]) ++k;
    const unsigned char* const triple = At(::bof3::Img(at::kTwinAnims.v) + 3 * k);
    SharedScratch.w_00 = self[3];
    SH_CALL(EventOp_0x)(At(::bof3::Img(at::kTwinOps.v) + 0x11u * self[0xB]));
    SH_CALL(Sprite_SetAnimationAt)(triple[1], triple[2]);
    SharedScratch.w_00 = self[4];
    SH_CALL(EventOp_0x)(At(::bof3::Img(at::kTwinOps.v) + 0x11u * self[0xB]));
    SH_CALL(Sprite_SetAnimationAt)(triple[1], triple[2]);
    Sprite_Current = self;
    unsigned char* a = SpriteRec(who, self[3]);
    SetUL(a + 0x34, UL(a + 0x34) + 0xFFFFF000u);
    SetUL(a + 0x38, UL(a + 0x38) + 0x1000u);
    unsigned char* b = SpriteRec(who, self[4]);
    SetUL(b + 0x34, UL(b + 0x34) + 0x1000u);
    const U height = UL(rv::Bytes(ObjTrio) + 0x3C);
    SetUL(b + 0x38, UL(b + 0x38) + 0xFFFFF000u);
    SetUL(b + 0x3C, height);
    SetUL(a + 0x3C, height);
    a = SpriteRec(who, self[3]);
    a[0] = static_cast<unsigned char>(a[0] | 0x20);
    b = SpriteRec(who, self[4]);
    b[0] = static_cast<unsigned char>(b[0] | 0x20);
    SpriteRec(who, self[4])[0x5C] = 0;
    SpriteRec(who, self[3])[0x5C] = 0;
    SH_CALL(Sprite_SetTint)(SpriteRec(who, self[3]), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(SpriteRec(who, self[4]), 0, 0, 0xF, 1);
    S()[9] = 4;
    S()[1] = 1;
}

// original 0x4780D0 (state 1): while +9 is not 0 the leader's +0 |= 0x40 (sprite
// object 2's when +0xB is 0) and +9 down; at 0 that bit cleared, both sprites'
// +0 = 0 and +1 = 2.
extern "C" void __cdecl EffectKind47_Blink(void) {
    unsigned char* const s = S();
    unsigned char* const model = s[0xB] != 0 ? rv::Bytes(ObjTrio) : At(::bof3::Img(Sprite_Objects_addr) + 0x148);
    if (s[9] != 0) {
        model[0] = static_cast<unsigned char>(model[0] | 0x40);
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    model[0] = static_cast<unsigned char>(model[0] & 0xBF);
    SpriteRec("EffectKind47_Blink", s[3])[0] = 0;
    SpriteRec("EffectKind47_Blink", s[4])[0] = 0;
    s[1] = 2;
}

// original 0x478160 (EffectKind47_States[2], EffectKind14_States[2],
// EffectKind3C_States[2]): Sprite_ReleaseTint for Sprite_Objects[+3] and
// [+4] (Sprite_Current read for each; the addresses only - the callee compares,
// it does not read the record), then Effect_Release (a tail jump).
extern "C" void __cdecl EffectTwinSprites_Release(void) {
    SH_CALL(Sprite_ReleaseTint)(rv::Bytes(&Sprite_Objects[S()[3]]));
    SH_CALL(Sprite_ReleaseTint)(rv::Bytes(&Sprite_Objects[S()[4]]));
    SH_CALL(Effect_Release)();
}

void EffectKind46_Inject() {
    BOF3_INJECT(EffectKind46_Run);
    BOF3_INJECT(EffectKind46_Start);
    BOF3_INJECT(EffectKind46_FadeIn);
    BOF3_INJECT(EffectKind46_FadeOut);
    BOF3_INJECT(EffectKind46_Count);
    BOF3_INJECT(EffectKind46_Hold);
    BOF3_INJECT(EffectKind46_DrawFlash);
    BOF3_INJECT(EffectKind46_RedrawSprites);
    BOF3_INJECT(EffectKind47_Run);
    BOF3_INJECT(EffectKind47_Start);
    BOF3_INJECT(EffectKind47_Blink);
    BOF3_INJECT(EffectTwinSprites_Release);
}
