// Moved here by tools/move_funcs.py from effect_4b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_9d.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_4b.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sprite_draw.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_4b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

unsigned char& Byte(U cell) { return At(cell)[0]; }

// `fld dword [a]; fadd dword [c]; fstp dword [o]`
void FldAdd(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%0)\n\tfadds (%1)\n\tfstps (%2)" : : "r"(a), "r"(c), "r"(o) : "st", "memory");
}

// `fld dword [a]; fsub dword [c]; fstp dword [o]`
void FldSub(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%0)\n\tfsubs (%1)\n\tfstps (%2)" : : "r"(a), "r"(c), "r"(o) : "st", "memory");
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the
// dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4b.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// ObjTrio record m; past the three, ours aborts (the original writes what
// follows).
unsigned char* Member(unsigned m, const char* who) {
    if (m >= at::kObjTrioCount)
        bof3::Fatal("%s: party member %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, m);
    return rv::Bytes(&ObjTrio[m]);
}

// Field_MemberCount as a loop bound over ObjTrio; past three, ours aborts.
unsigned Members(const char* who) {
    const unsigned n = Field_MemberCount;
    if (n > at::kObjTrioCount)
        bof3::Fatal("%s: Field_MemberCount is %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, n);
    return n;
}

// Sprite_Objects record k (the effect record's +0xB); past the thirty, ours
// aborts.
unsigned char* Sprite(unsigned k, const char* who) {
    if (k >= at::kSpriteCount)
        bof3::Fatal("%s: +0xB is %u, past Sprite_Objects' thirty records - the original reads and writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, k);
    return rv::Bytes(&Sprite_Objects[k]);
}

// Field_Request 3 or 1: the object's +0x24 bit 3 cleared, +1 = 0.
void Kind9DLetGo(unsigned char* s, const char* who) {
    unsigned char* const o = Sprite(s[0xB], who);
    o[0x24] = static_cast<unsigned char>(o[0x24] & 0xF7);
    s[1] = 0;
}

// Kind 0x9D's quad (0x48A010 and 0x48A160): a POLY_GT4 at the cursor (read
// once), Gte_PrimDepthFlat4_14; its corners (x, y), (x + kQuadW, y), (x, y +
// kQuadH), (x + kQuadW, y + kQuadH) - the sums in the FPU; its texture the
// record's u +0x2E and v +0x30 -/+ 0xC and -0x25 / +0xB; `top` the two upper
// corners' grey, `left` / `right` the lower two's green; a page (2, abr 1,
// (0x340, 0x100)); semi-transparent; linked at the record's point (dy 1,
// 0x54).
void Kind9DQuad(U x, U y, int top, int left, int right) {
    unsigned char* const q = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyGT4)(q);
    SH_CALL(Gte_PrimDepthFlat4_14)(q);
    float x1, y1;
    FldAdd(&x, At(at::kQuadW), &x1);
    FldAdd(&y, At(at::kQuadH), &y1);
    SetUL(q + 0xC, y);
    SetUL(q + 8, x);
    SetUL(q + 0x20, y);
    std::memcpy(q + 0x1C, &x1, 4);
    SetUL(q + 0x30, x);
    std::memcpy(q + 0x34, &y1, 4);
    std::memcpy(q + 0x44, &x1, 4);
    std::memcpy(q + 0x48, &y1, 4);
    q[0x14] = static_cast<unsigned char>(S()[0x2E] - 0xC);
    q[0x15] = static_cast<unsigned char>(S()[0x30] - 0x25);
    q[0x28] = static_cast<unsigned char>(S()[0x2E] + 0xC);
    q[0x29] = static_cast<unsigned char>(S()[0x30] - 0x25);
    q[0x3C] = static_cast<unsigned char>(S()[0x2E] - 0xC);
    q[0x3D] = static_cast<unsigned char>(S()[0x30] + 0xB);
    q[0x50] = static_cast<unsigned char>(S()[0x2E] + 0xC);
    q[0x51] = static_cast<unsigned char>(S()[0x30] + 0xB);
    q[4] = q[5] = q[6] = static_cast<unsigned char>(top);
    q[0x18] = q[0x19] = q[0x1A] = static_cast<unsigned char>(top);
    if (left < 0) {
        // the glow: both lower corners `right` grey
        q[0x2C] = q[0x2D] = q[0x2E] = static_cast<unsigned char>(right);
        q[0x40] = q[0x41] = q[0x42] = static_cast<unsigned char>(right);
    } else {
        // the pulse: the lower corners green only
        q[0x2C] = 0;
        q[0x2D] = static_cast<unsigned char>(left);
        q[0x2E] = 0;
        q[0x40] = 0;
        q[0x41] = static_cast<unsigned char>(right);
        q[0x42] = 0;
    }
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(2, 1, 0x340, 0x100);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyGT4>(q)->tpage, tpage);
    SH_CALL(Gpu_SetSemiTrans)(q, 1);
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), 1, 0x54);
}

}  // namespace

// original 0x489D70 (Effect_KindHandlers[0x9D], hidden after 0x489CD0):
// jmp [EffectKind9D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9D_Run(void) {
    Dispatch("EffectKind9D_Run", AddressOf(EffectKind9D_States), EffectKind9D_States_count);
}

namespace effect_4b { std::uint32_t g_hologram_clear = 0; }   // DIV-0142's amendment; 1 through PatchBytes("SpriteCapture") in Effect4B_Inject

// original 0x489D90 (state 0): nothing while Field_Request is 3 or 1. Else
// the field object of +0xB: its +0x24 bit 3 set, its words +0x2E / +0x30 the
// record's, its point +0x34.. the record's point; the colour +0x5D..+0x5F 0;
// +6 0 - the pulse at once (EffectKind9D_Pulse) and +1 = 1; else +1 = 2.
//
// DIV-0142's amendment (issue #191): first, the rectangle the quad reads
// (page (0x340, 0x100), u +0x2E - 0xC, v +0x30 - 0x25, as Kind9DQuad takes
// them) is cleared - 25 x 49, both far edges included: the quad's far corners
// are u +0xC and v +0xB, a column and a row the PlayStation never samples but
// the PC's D3D draw does at some sub-pixel positions (2026-11-07, a stripe at
// the right that came and went as the camera turned). The PlayStation's page is empty there in areas 173
// and 174 (a Mednafen VRAM dump, 2026-11-03); the PC's START.DAT leaves its
// title art at (0x350..0x3BF, 0x160..0x1BF), which the hologram drew as a box.
extern "C" void __cdecl EffectKind9D_Start(void) {
    const unsigned char request = Field_Request;
    if (request == 3 || request == 1) return;
    if (effect_4b::g_hologram_clear != 0)
        SpriteCapture_ClearSlot(0x340 + static_cast<unsigned char>(S()[0x2E] - 0xC),
                                0x100 + static_cast<unsigned char>(S()[0x30] - 0x25), 0x19, 0x31);
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    unsigned char* const o = Sprite(s[0xB], "EffectKind9D_Start");
    auto* const oo = rv::As<records::SpriteObject>(o);
    o[0x24] = static_cast<unsigned char>(o[0x24] | 8);
    oo->screen_x = rv::As<records::EffectWork>(s)->work_2e;
    oo->screen_y = rv::As<records::EffectWork>(s)->work_30;
    so->x = static_cast<std::uint32_t>(oo->x);
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(oo->z));
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(oo->height));
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    unsigned char* const s2 = S();
    if (s2[6] == 0) {
        SH_CALL(EffectKind9D_Pulse)();
        S()[1] = 1;
        return;
    }
    s2[1] = 2;
}

// original 0x489E50 (state 1): Field_Request 3 or 1 - let go (+1 = 0). Else
// projected (EffectKind9D_Project); +9 up 8; +0x5D / +0x5E Math_Sin / Math_Cos(+9
// << 4) sar 7; the pulse drawn at the projected point.
extern "C" void __cdecl EffectKind9D_Pulse(void) {
    const unsigned char request = Field_Request;
    if (request == 3 || request == 1) {
        Kind9DLetGo(S(), "EffectKind9D_Pulse");
        return;
    }
    float out[2];
    SH_CALL(EffectKind9D_Project)(out);
    S()[9] = static_cast<unsigned char>(S()[9] + 8);
    const int sn = SH_CALL(Math_Sin)(static_cast<int>(static_cast<U>(S()[9]) << 4));
    S()[0x5D] = static_cast<unsigned char>(Sar(static_cast<U>(sn), 7));
    const int cs = SH_CALL(Math_Cos)(static_cast<int>(static_cast<U>(S()[9]) << 4));
    S()[0x5E] = static_cast<unsigned char>(Sar(static_cast<U>(cs), 7));
    U x, y;
    std::memcpy(&x, &out[0], 4);
    std::memcpy(&y, &out[1], 4);
    SH_CALL(EffectKind9D_DrawPulse)(x, y);
}

// original 0x489F00 (state 2): projected; +0x5D up 2 below 0x70 and +0x5E up 1
// below 0x30 (signed); the glow drawn; Field_Request 3 or 1 - let go; then at
// (0x70, 0x30) exactly, +6 0 and +1 = 1.
extern "C" void __cdecl EffectKind9D_Grow(void) {
    float out[2];
    SH_CALL(EffectKind9D_Project)(out);
    unsigned char* s = S();
    if (static_cast<signed char>(s[0x5D]) < 0x70) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 2);
        s = S();
    }
    if (static_cast<signed char>(s[0x5E]) < 0x30) s[0x5E] = static_cast<unsigned char>(s[0x5E] + 1);
    U x, y;
    std::memcpy(&x, &out[0], 4);
    std::memcpy(&y, &out[1], 4);
    SH_CALL(EffectKind9D_DrawGlow)(x, y);
    const unsigned char request = Field_Request;
    if (request == 3 || request == 1) Kind9DLetGo(S(), "EffectKind9D_Grow");
    s = S();
    if (s[0x5D] == 0x70 && s[0x5E] == 0x30) {
        s[6] = 0;
        S()[1] = 1;
    }
}

// original 0x489FA0 (state 3): projected; +0x5D down 2 and +0x5E down 1 while
// not 0; the glow drawn; both 0: Effect_Release.
extern "C" void __cdecl EffectKind9D_Shrink(void) {
    float out[2];
    SH_CALL(EffectKind9D_Project)(out);
    unsigned char* s = S();
    if (s[0x5D] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
        s = S();
    }
    if (s[0x5E] != 0) s[0x5E] = static_cast<unsigned char>(s[0x5E] - 1);
    U x, y;
    std::memcpy(&x, &out[0], 4);
    std::memcpy(&y, &out[1], 4);
    SH_CALL(EffectKind9D_DrawGlow)(x, y);
    s = S();
    if (s[0x5D] == 0 && s[0x5E] == 0) SH_CALL(Effect_Release)();
}

// original 0x48A010 (x, y: a screen point's floats): Kind9DQuad with the upper
// corners 0x90 grey and the lower two green +0x5D + 0x20 / +0x5E + 0x20 (both
// read at entry).
extern "C" void __cdecl EffectKind9D_DrawPulse(unsigned long x, unsigned long y) {
    unsigned char* const s = S();
    const unsigned char a = static_cast<unsigned char>(s[0x5D] + 0x20);
    const unsigned char b = static_cast<unsigned char>(s[0x5E] + 0x20);
    Kind9DQuad(static_cast<U>(x), static_cast<U>(y), 0x90, a, b);
}

// original 0x48A160 (x, y): Kind9DQuad with the upper corners +0x5D grey and
// the lower two +0x5E grey (both read at entry).
extern "C" void __cdecl EffectKind9D_DrawGlow(unsigned long x, unsigned long y) {
    unsigned char* const s = S();
    const unsigned char a = s[0x5D];
    const unsigned char b = s[0x5E];
    Kind9DQuad(static_cast<U>(x), static_cast<U>(y), a, -1, b);
}

// original 0x48A2A0 (out: two floats): the field object of +0xB given the
// record's words +0x2E / +0x30; the vector ((+0x34 sar 9) - 0x4000, (+0x38 sar
// 9) - 0x4000, -(+0x3E / 2)) through Gte_RotTransPers (its fourth word Capcom
// never wrote; the original pushes its own argument's slot as a fourth
// argument the callee does not read), the depth to +0x60; out less kCentreX /
// kCentreY.
extern "C" void __cdecl EffectKind9D_Project(float* out) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    unsigned char* const o = Sprite(s[0xB], "EffectKind9D_Project");
    auto* const oo = rv::As<records::SpriteObject>(o);
    oo->screen_x = rv::As<records::EffectWork>(s)->work_2e;
    oo->screen_y = rv::As<records::EffectWork>(s)->work_30;
    short v[3];
    v[0] = static_cast<short>(Sar(static_cast<std::uint32_t>(so->x), 9) - 0x4000u);
    v[1] = static_cast<short>(Sar(static_cast<std::uint32_t>(so->z), 9) - 0x4000u);
    v[2] = static_cast<short>(-(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->height_whole)) / 2));
    long depth;
    const long r = SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(out), &depth);
    rv::Current<records::EffectObject>()->depth = static_cast<U>(r);
    FldSub(&out[0], At(at::kCentreX), &out[0]);
    FldSub(&out[1], At(at::kCentreY), &out[1]);
}

// original 0x48A350 (Effect_KindHandlers[0x9F], hidden after 0x48A2A0):
// jmp [EffectKind9F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9F_Run(void) {
    Dispatch("EffectKind9F_Run", AddressOf(EffectKind9F_States), EffectKind9F_States_count);
}

// original 0x48A370 (state 0 of kinds 0x9F and 0xA4): the party members below
// Field_MemberCount (read once) at pose +0x29 3; the colour +0x5D..+0x5F 0; +1
// = 1.
extern "C" void __cdecl EffectKind9F_Start(void) {
    const unsigned n = Members("EffectKind9F_Start");
    for (unsigned i = 0; i < n; ++i) Member(i, "EffectKind9F_Start")[0x29] = 3;
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[1] = 1;
}

// original 0x48A3C0 (state 1): +0x5D below 0x7C (signed) - the colour up 4
// each; else the counter 0x903848 up and +1 = 2. The tile; the party tinted (a
// tail jump).
extern "C" void __cdecl EffectKind9F_Brighten(void) {
    unsigned char* const s = S();
    const unsigned char c = s[0x5D];
    if (static_cast<signed char>(c) < 0x7C) {
        s[0x5D] = static_cast<unsigned char>(c + 4);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 4);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 4);
    } else {
        Byte(::bof3::Img(at::kCounter.v)) = static_cast<unsigned char>(Byte(::bof3::Img(at::kCounter.v)) + 1);
        s[1] = 2;
    }
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKind9F_TintParty)();
}

// original 0x48A410 (state 2): the tile; the party tinted (a tail jump).
extern "C" void __cdecl EffectKind9F_Hold(void) {
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKind9F_TintParty)();
}

// original 0x48A420 (state 3): +0x5D above 0 - the colour down 2 each, the
// tile, the party tinted (a tail jump). Else the party members below
// Field_MemberCount (read once) given Draw_OtSlot (read once) as pose +0x29,
// and Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind9F_Fade(void) {
    unsigned char* const s = S();
    const unsigned char c = s[0x5D];
    if (c != 0) {
        s[0x5D] = static_cast<unsigned char>(c - 2);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 0xFE);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 0xFE);
        SH_CALL(EffectKind89_DrawTint)();
        SH_CALL(EffectKind9F_TintParty)();
        return;
    }
    const unsigned n = Members("EffectKind9F_Fade");
    if (n != 0) {
        const unsigned char slot = Draw_OtSlot;
        for (unsigned i = 0; i < n; ++i) Member(i, "EffectKind9F_Fade")[0x29] = slot;
    }
    SH_CALL(Effect_Release)();
}

// original 0x48A480 (the tail of states 1..3, its own ret): v = -(s8 +0x5D /
// 2), truncating; each party member below Field_MemberCount (read once) not
// at +0x89 7 given v as its colour +0x5D..+0x5F.
extern "C" void __cdecl EffectKind9F_TintParty(void) {
    const std::int32_t c = static_cast<signed char>(S()[0x5D]);
    const auto v = static_cast<unsigned char>(-(c / 2));
    const unsigned n = Members("EffectKind9F_TintParty");
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const m = Member(i, "EffectKind9F_TintParty");
        if (m[0x89] == 7) continue;
        m[0x5F] = v;
        m[0x5E] = v;
        m[0x5D] = v;
    }
}

// original 0x48A4C0 (Effect_KindHandlers[0xA4], hidden after 0x48A2A0):
// jmp [EffectKindA4_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKindA4_Run(void) {
    Dispatch("EffectKindA4_Run", AddressOf(EffectKindA4_States), EffectKindA4_States_count);
}

// original 0x48A4E0 (state 1): +0x5D below 0xF8 (unsigned) - the colour up 8
// each; else all 0xFF, the chapter's step byte up and +1 = 2. The tile; the
// marked objects drawn (a tail jump).
extern "C" void __cdecl EffectKindA4_Brighten(void) {
    unsigned char* const s = S();
    const unsigned char c = s[0x5D];
    if (c < 0xF8) {
        s[0x5D] = static_cast<unsigned char>(c + 8);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 8);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 8);
    } else {
        s[0x5F] = 0xFF;
        S()[0x5E] = 0xFF;
        S()[0x5D] = 0xFF;
        MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
        S()[1] = 2;
    }
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKindA4_MarkSprites)();
}

// original 0x48A550 (state 2; not in the cut): the tile; the marked objects
// drawn (a tail jump).
extern "C" void __cdecl EffectKindA4_Hold(void) {
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKindA4_MarkSprites)();
}

// original 0x48A560 (the tail of states 1 and 2, its own frame and ret): each
// of the thirty field objects whose record word (Scena15_RecordWord(its word
// +0x2C)) is 0x261, and each whose word (asked again) is 0x308 while flag 0x11
// of the chapter's row is set, made Sprite_Current, pose +0x29 3,
// Sprite_UpdateScreen; Sprite_Current put back.
extern "C" void __cdecl EffectKindA4_MarkSprites(void) {
    unsigned char* const saved = Sprite_Current;
    for (unsigned i = 0; i < at::kSpriteCount; ++i) {
        unsigned char* const o = rv::Bytes(&Sprite_Objects[i]);
        auto* const oo = rv::As<records::SpriteObject>(o);
        if ((SH_CALL(Scena15_RecordWord)(oo->column) & 0xFFFFu) == 0x261) {
            Sprite_Current = o;
            o[0x29] = 3;
            SH_CALL(Sprite_UpdateScreen)();
        }
        if ((SH_CALL(Scena15_RecordWord)(oo->column) & 0xFFFFu) == 0x308 &&
            SH_CALL(Flags_Test)(reinterpret_cast<const unsigned char*>(Chapter_FlagRow), 0x11) != 0) {
            Sprite_Current = o;
            o[0x29] = 3;
            SH_CALL(Sprite_UpdateScreen)();
        }
    }
    Sprite_Current = saved;
}

void EffectKind9d_Inject() {
    BOF3_INJECT(EffectKind9D_Run);
    BOF3_INJECT(EffectKind9D_Start);
    BOF3_INJECT(EffectKind9D_Pulse);
    BOF3_INJECT(EffectKind9D_Grow);
    BOF3_INJECT(EffectKind9D_Shrink);
    BOF3_INJECT(EffectKind9D_DrawPulse);
    BOF3_INJECT(EffectKind9D_DrawGlow);
    BOF3_INJECT(EffectKind9D_Project);
    BOF3_INJECT(EffectKind9F_Run);
    BOF3_INJECT(EffectKind9F_Start);
    BOF3_INJECT(EffectKind9F_Brighten);
    BOF3_INJECT(EffectKind9F_Hold);
    BOF3_INJECT(EffectKind9F_Fade);
    BOF3_INJECT(EffectKind9F_TintParty);
    BOF3_INJECT(EffectKindA4_Run);
    BOF3_INJECT(EffectKindA4_Brighten);
    BOF3_INJECT(EffectKindA4_Hold);
    BOF3_INJECT(EffectKindA4_MarkSprites);
    {
        // DIV-0142's amendment (issue #191): armed after the fuzz, which compares Capcom's start.
        const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
        bof3::PatchBytes("SpriteCapture", Low32(&effect_4b::g_hologram_clear), was, is, 4);
    }
}
