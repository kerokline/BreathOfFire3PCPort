// Moved here by tools/move_funcs.py from effect_5a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_5a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

unsigned char T(U a) { return *At(a); }

std::int32_t TS(U a) { return static_cast<signed char>(*At(a)); }

U TL(U a) { return static_cast<U>(Long(At(a))); }

std::int32_t Sar(U v, unsigned n) { return static_cast<std::int32_t>(v) >> n; }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, bytes or a null.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5a.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(TL(table + 4 * sub)))();
}

// An index into one of the image's tables by a byte or word of the record,
// unchecked by the original (it reads whatever follows).
std::int32_t Index(const char* who, std::int32_t i, unsigned n, U table) {
    if (i < 0 || static_cast<unsigned>(i) >= n)
        bof3::Fatal("%s: index %d into 0x%X, past its %u entries - the original reads what follows "
                    "(docs/effect_5a.md section 7)",
                    who, (int)i, (unsigned)table, n);
    return i;
}

// Prim_VertexScratch's four SVECTORs (x, y, z words at +0, +2, +4; the pads
// +6 left as they are).
// kV: now Prim_VertexScratch (symbols.toml).
void Vertex(unsigned k, U x, U y, U z) {
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k), x & 0xFFFF);
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k + 2), y & 0xFFFF);
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k + 4), z & 0xFFFF);
}

// Gte_RotTransPers4 of the four into the POLY_FT4's corners (the tenth word
// Capcom pushes, a second local, is read by no Gte_RotTransPers4 of ours),
// then Gte_PrimDepths4_10.
void Project4(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 8)),
                               reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x10)), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x18)),
                               reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// A POLY_FT4 begun at Gfx_PacketNext: Gpu_SetPolyFT4, Gpu_SetShadeTex(0).
unsigned char* BeginQuad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}

// The GTE matrix pushed, then a MATRIX on the stack: its translation
// Gte_RotTrans(point) (+0x14), its rotation Gte_RotMatrix(angles), multiplied
// by Camera_Matrix (Gte_MulMatrix0(camera, m, m)), set as the rotation and
// the translation. Capcom's third word to Gte_RotTrans (a flag out) is read by
// no Gte_RotTrans of ours.
void PushMatrix(const short* point, const short* angles) {
    SH_CALL(Gte_PushMatrix)();
    alignas(4) unsigned char m[0x20];
    SH_CALL(Gte_RotTrans)(point, reinterpret_cast<long*>(m + 0x14));
    SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(m));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(m), reinterpret_cast<short*>(m));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(m));
}

// --- x87 as the original has it (the game's control word) ---------------------
// `fild dword [n]; fsubr dword [c]; fst dword [o1]; fstp dword [o2]`
void FildSubr2(std::int32_t n, const void* c, void* o1, void* o2) {
    __asm__ volatile("fildl %3\n\tfsubrs (%2)\n\tfsts (%0)\n\tfstps (%1)" : : "r"(o1), "r"(o2), "r"(c), "m"(n) : "st", "memory");
}

// `fild dword [n]; fadd dword [c]; fst dword [o1]; fstp dword [o2]`
void FildAdd2(std::int32_t n, const void* c, void* o1, void* o2) {
    __asm__ volatile("fildl %3\n\tfadds (%2)\n\tfsts (%0)\n\tfstps (%1)" : : "r"(o1), "r"(o2), "r"(c), "m"(n) : "st", "memory");
}

// (v << 12) / 256 as the original computes it (cdq; and edx, 0xFF; add; sar 8).
U Div256(U v) {
    const std::int32_t x = static_cast<std::int32_t>(v << 12);
    return static_cast<U>((x + ((x >> 31) & 0xFF)) >> 8);
}

// --- sub-kind 5's test and rows ---------------------------------------------------
// The low nibble of the word +0x3A against ((+0x3A sar 4) & (the story-flag
// dword sar 4)): equal is "on".
bool On(const unsigned char* s) {
    const std::int32_t v = SW(s + 0x3A);
    const std::int32_t f = Long(At(::bof3::Img(Cond_Flags_addr) + 0xa0));
    return ((((v >> 4) & (f >> 4)) ^ v) & 0xF) == 0;
}

unsigned RowA(const char* who, const unsigned char* s) { return T(::bof3::Img(at::kRowsA.v) + Index(who, SW(s + 0x36), at::kRows, at::kRowsA.v)); }

unsigned RowB(const char* who, const unsigned char* s) { return T(::bof3::Img(at::kRowsB.v) + Index(who, SW(s + 0x36), at::kRows, at::kRowsB.v)); }

void Copy(U a, U b, U c, U d) { SH_CALL(Gfx_ClutStripCopy16)(a, b, c, d); }

// --- sub-kind 7 / 8's door ------------------------------------------------------------
unsigned Door(const char* who, const unsigned char* s) { return static_cast<unsigned>(Index(who, s[0xB], at::kDoors7, at::kRows7.v)); }

bool DoorFlag(unsigned k) {
    return SH_CALL(Flags_Test)(At(::bof3::Img(0x903F90) + 8u * T(::bof3::Img(at::kFlagRows7.v) + k)), T(::bof3::Img(at::kFlagBits7.v) + k)) != 0;
}

void DrawDoor() {
    SH_CALL(EffectKind18_07_Draw)();
    SH_CALL(EffectKind18_07_DrawTop)();
}

}  // namespace

// original 0x4FD470 (EffectKind18_States[4]): jmp through EffectKind18_04_States by +2.
extern "C" void __cdecl EffectKind18_04_Run(void) {
    Dispatch("EffectKind18_04_Run", AddressOf(EffectKind18_04_States), EffectKind18_04_States_count);
}

// original 0x4FD490 (state 0): flag 0x17 of Cond_Flags row 2 set: +2 3 and the
// open draw; else +2 up one and the shut draw (tail jumps).
extern "C" void __cdecl EffectKind18_04_Start(void) {
    const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x10), 0x17);
    unsigned char* const s = S();
    if (set != 0) {
        s[2] = 3;
        SH_CALL(EffectKind18_04_Open)();
        return;
    }
    s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18_04_Shut)();
}

// original 0x4FD4C0 (state 1): the flag set: +2 up one, +0xC 0. The two
// panels shut (angles 0 and 0x800).
extern "C" void __cdecl EffectKind18_04_Shut(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x10), 0x17) != 0) {
        unsigned char* const s = S();
        s[2] = static_cast<unsigned char>(s[2] + 1);
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    }
    short v[4];
    v[0] = static_cast<short>(0xE340);
    v[1] = static_cast<short>(0xE3C0);
    v[2] = 0;
    SH_CALL(EffectKind18_04_DrawPanel)(v, 0, at::kPanelTexture);
    SH_CALL(MapView_LinkPrimAt)(at::kPanelX, at::kPanelZ, 0, 0x48);
    v[0] = static_cast<short>(0xE340);
    v[1] = static_cast<short>(0xE4C0);
    v[2] = 0;
    SH_CALL(EffectKind18_04_DrawPanel)(v, 0x800, at::kPanelTexture2);
    SH_CALL(MapView_LinkPrimAt)(at::kPanelX, at::kPanelZ2, -1, 0x48);
}

// original 0x4FD570 (state 2): +0xC at 0x400 or past (signed): +2 up one;
// +0xC + 0x20; the panels at +0xC and 0x800 - +0xC.
extern "C" void __cdecl EffectKind18_04_Swing(void) {
    unsigned char* s = S();
    if (static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c) >= 0x400) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        s = S();
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + 0x20);
    short v[4];
    v[0] = static_cast<short>(0xE340);
    v[1] = static_cast<short>(0xE3C0);
    v[2] = 0;
    SH_CALL(EffectKind18_04_DrawPanel)(v, rv::As<records::EffectWork>(S())->work_0c, at::kPanelTexture);
    SH_CALL(MapView_LinkPrimAt)(at::kPanelX, at::kPanelZ, -1, 0x48);
    const U angle = (0x800u - Word(S() + 0xC)) & 0xFFFF;
    v[0] = static_cast<short>(0xE340);
    v[1] = static_cast<short>(0xE4C0);
    v[2] = 0;
    SH_CALL(EffectKind18_04_DrawPanel)(v, angle, at::kPanelTexture2);
    SH_CALL(MapView_LinkPrimAt)(at::kPanelX, at::kPanelZ2, -1, 0x48);
}

// original 0x4FD630 (state 3): a draw mode (0x95) linked at the first panel's
// cell (size 0xC), the one panel at 0x400.
extern "C" void __cdecl EffectKind18_04_Open(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(MapView_LinkPrimAt)(at::kPanelX, at::kPanelZ, -1, 0xC);
    short v[4];
    v[0] = static_cast<short>(0xE340);
    v[1] = static_cast<short>(0xE3C0);
    v[2] = 0;
    SH_CALL(EffectKind18_04_DrawPanel)(v, 0x400, at::kPanelTexture);
    SH_CALL(MapView_LinkPrimAt)(at::kPanelX, at::kPanelZ, -1, 0x48);
}

// original 0x4FD6A0 (cdecl, three words): the panel at `point` turned by the
// angle's word about z: a POLY_FT4 of (0, 0x80, -0x17E), (0, 0, -0x17E),
// (0, 0x80, 0), (0, 0, 0); the matrix popped, then the texture. Answers the
// primitive.
extern "C" unsigned char* __cdecl EffectKind18_04_DrawPanel(const short* point, unsigned angle, unsigned long texture) {
    short angles[4];
    angles[0] = 0;
    angles[1] = 0;
    angles[2] = static_cast<short>(angle);
    PushMatrix(point, angles);
    unsigned char* const p = BeginQuad();
    Vertex(0, 0, 0x80, 0xFFFFFE82u);
    Vertex(1, 0, 0, 0xFFFFFE82u);
    Vertex(2, 0, 0x80, 0);
    Vertex(3, 0, 0, 0);
    Project4(p);
    SH_CALL(Gte_PopMatrix)();
    SH_CALL(Prim_SetTexture)(texture, p, 1);
    return p;
}

// original 0x4FD7E0 (EffectKind18_States[5]).
extern "C" void __cdecl EffectKind18_05_Run(void) {
    Dispatch("EffectKind18_05_Run", AddressOf(EffectKind18_05_States), EffectKind18_05_States_count);
}

// original 0x4FD800 (state 0): on: +2 3, row a's slot 9 to slot 0; else +2 up
// one, slot 1; then row b's slot 8 to slot 0.
extern "C" void __cdecl EffectKind18_05_Start(void) {
    static const char kWho[] = "EffectKind18_05_Start";
    unsigned char* s = S();
    if (On(s)) {
        s[2] = 3;
        const unsigned a = RowA(kWho, S());
        Copy(a, 9, a, 0);
    } else {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        const unsigned a = RowA(kWho, S());
        Copy(a, 1, a, 0);
    }
    const unsigned b = RowB(kWho, S());
    Copy(b, 8, b, 0);
}

// original 0x4FD890 (state 1): row a's slot 1, row b's slot 8; on: +9, +0xA 0,
// +2 up one.
extern "C" void __cdecl EffectKind18_05_Idle(void) {
    static const char kWho[] = "EffectKind18_05_Idle";
    const unsigned a = RowA(kWho, S());
    Copy(a, 1, a, 0);
    const unsigned b = RowB(kWho, S());
    Copy(b, 8, b, 0);
    unsigned char* const s = S();
    if (!On(s)) return;
    s[9] = 0;
    S()[0xA] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FD910 (state 2): every fifth frame row b's slot 8 - +9 / 5; +9
// up; +0xA below 9: row a's slot +0xA + 1; +0xA up; +9 past 0x1E: +9, +0xA 0,
// +2 up one.
extern "C" void __cdecl EffectKind18_05_FadeIn(void) {
    static const char kWho[] = "EffectKind18_05_FadeIn";
    unsigned char* s = S();
    const unsigned n = s[9];
    if (n % 5 == 0) {
        const unsigned b = RowB(kWho, s);
        Copy(b, 8u - n / 5, b, 0);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    const unsigned k = s[0xA];
    if (k < 9) {
        const unsigned a = RowA(kWho, s);
        Copy(a, k + 1, a, 0);
        s = S();
    }
    s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    s = S();
    if (s[9] > 0x1E) {
        s[9] = 0;
        S()[0xA] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x4FD9D0 (state 3): +9 & 3 0: row b's slot 0x65DB18[(+9 >> 2) % 6];
// row a's slot 9; not on and (+9 >> 2) % 6 0: +9, +0xA 0, +2 up one; +9 up.
extern "C" void __cdecl EffectKind18_05_Pulse(void) {
    static const char kWho[] = "EffectKind18_05_Pulse";
    unsigned char* s = S();
    const unsigned n = s[9];
    if ((n & 3) == 0) {
        const unsigned b = RowB(kWho, s);
        Copy(b, T(::bof3::Img(at::kPulseSlots.v) + (n >> 2) % 6), b, 0);
        s = S();
    }
    const unsigned a = RowA(kWho, s);
    Copy(a, 9, a, 0);
    s = S();
    if (!On(s) && (static_cast<unsigned>(s[9]) >> 2) % 6 == 0) {
        s[9] = 0;
        S()[0xA] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        S()[9] = static_cast<unsigned char>(S()[9] + 1);
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

// original 0x4FDAA0 (state 4): every fifth frame row b's slot +9 / 5 + 2; +9
// up; +0xA even and below 9: row a's slot 0x65DB20[+0xA >> 1]; +0xA up; +9
// past 0x1E: +9, +0xA 0, +2 1.
extern "C" void __cdecl EffectKind18_05_FadeOut(void) {
    static const char kWho[] = "EffectKind18_05_FadeOut";
    unsigned char* s = S();
    const unsigned n = s[9];
    if (n % 5 == 0) {
        const unsigned b = RowB(kWho, s);
        Copy(b, n / 5 + 2, b, 0);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    const unsigned k = s[0xA];
    if ((k & 1) == 0 && k < 9) {
        const unsigned a = RowA(kWho, s);
        Copy(a, T(::bof3::Img(at::kFadeSlots.v) + (k >> 1)), a, 0);
        s = S();
    }
    s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    s = S();
    if (s[9] > 0x1E) {
        s[9] = 0;
        S()[0xA] = 0;
        S()[2] = 1;
    }
}

// original 0x4FDB70 (cdecl, four words): a 16-colour slot of the CLUT strip
// copied (8 dwords, from the first word up) and the strip marked dirty.
extern "C" void __cdecl Gfx_ClutStripCopy16(unsigned from_row, unsigned from_slot, unsigned to_row, unsigned to_slot) {
    const U from = ((from_row << 4) + from_slot) << 5;
    const U to = ((to_row << 4) + to_slot) << 5;
    if (from > at::kClutStripSize - 0x20 || to > at::kClutStripSize - 0x20)
        bof3::Fatal("Gfx_ClutStripCopy16: (%d, %d) -> (%d, %d) lies outside the CLUT strip - the original copies "
                    "there unchecked (docs/effect_5a.md section 7)",
                    (int)from_row, (int)from_slot, (int)to_row, (int)to_slot);
    for (U i = 0; i < 0x20; i += 4) SetLong(At(::bof3::Img(Gfx_ClutStrip_addr) + to + i), Long(At(::bof3::Img(Gfx_ClutStrip_addr) + from + i)));
    Gfx_ClutStripDirty = 1;
}

// original 0x4FDBC0 (EffectKind18_States[26]).
extern "C" void __cdecl EffectKind18_1A_Run(void) {
    Dispatch("EffectKind18_1A_Run", AddressOf(EffectKind18_1A_States), EffectKind18_1A_States_count);
}

// original 0x4FDBE0 (state 0): +0x3E 0x320, +2 up one; the draw.
extern "C" void __cdecl EffectKind18_1A_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0x320);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18_1A_Draw)();
}

// original 0x4FDC00 (state 1): Cond_ByteFE not 0: +2 up one; the draw.
extern "C" void __cdecl EffectKind18_1A_WaitOn(void) {
    if (Cond_ByteFE != 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18_1A_Draw)();
}

// original 0x4FDC20 (state 2): +0x38 + 0x2000, +0x3E + 0x10; +0x38 0x350000:
// +2 up one; the draw.
extern "C" void __cdecl EffectKind18_1A_SlideOut(void) {
    rv::Current<records::EffectObject>()->z = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) + 0x2000;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, rv::As<records::EffectObject>(S())->height_whole + 0x10u);
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    if (static_cast<std::uint32_t>(so->z) == 0x350000) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18_1A_Draw)();
}

// original 0x4FDC60 (state 3): Cond_ByteFE 0: +2 up one; the draw.
extern "C" void __cdecl EffectKind18_1A_WaitOff(void) {
    if (Cond_ByteFE == 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18_1A_Draw)();
}

// original 0x4FDC80 (state 4): +0x38 - 0x2000, +0x3E - 0x10; +0x38 0x340000:
// +2 1; the draw.
extern "C" void __cdecl EffectKind18_1A_SlideBack(void) {
    rv::Current<records::EffectObject>()->z = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) - 0x2000;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, rv::As<records::EffectObject>(S())->height_whole - 0x10u);
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    if (static_cast<std::uint32_t>(so->z) == 0x340000) s[2] = 1;
    SH_CALL(EffectKind18_1A_Draw)();
}

// original 0x4FDCC0 (the five states' draw): a POLY_FT4 about the record's
// cell, its height +0x3E -/+ 0x30, linked at (0x5D0000, 0x340000).
extern "C" void __cdecl EffectKind18_1A_Draw(void) {
    unsigned char* s = S();
    const U z = static_cast<U>(Sar(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 9) - 0x4000);
    unsigned char* const p = Gfx_PacketNext;
    const U x = static_cast<U>(Sar(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), 9) - 0x4000);
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    s = S();
    const U h = static_cast<std::uint16_t>(rv::As<records::EffectObject>(s)->height_whole);
    Vertex(0, x - 0x40, z - 0x30, h - 0x30);
    Vertex(1, x + 0xC0, z - 0x30, h - 0x30);
    Vertex(2, x - 0x40, z + 0x30, h + 0x30);
    Vertex(3, x + 0xC0, z + 0x30, h + 0x30);
    Project4(p);
    SH_CALL(Prim_SetTexture)(0x21800133, p, 1);
    SH_CALL(MapView_LinkPrimAt)(0x5D0000, 0x340000, 0, 0x48);
}

// original 0x4FDDD0 (EffectKind18_States[31]).
extern "C" void __cdecl EffectKind18_1F_Run(void) {
    Dispatch("EffectKind18_1F_Run", AddressOf(EffectKind18_1F_States), EffectKind18_1F_States_count);
}

// original 0x4FDDF0 (state 0): +9 0; story flag 7 set: +2 3; else slot (3, 0)
// to (3, 3) and +2 up one.
extern "C" void __cdecl EffectKind18_1F_Start(void) {
    S()[9] = 0;
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 7) != 0) {
        S()[2] = 3;
        return;
    }
    Copy(3, 0, 3, 3);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FDE30 (state 1): story flag 7 set: +2 up one.
extern "C" void __cdecl EffectKind18_1F_Wait(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 7) != 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FDE50 (state 2): (3, (+9 >> 1) + 5) to (3, 3), (6, (+9 >> 1) +
// 0xB) to (6, 9); +9 up; past 5: +2 up one.
extern "C" void __cdecl EffectKind18_1F_Brighten(void) {
    Copy(3, (static_cast<unsigned>(S()[9]) >> 1) + 5, 3, 3);
    Copy(6, (static_cast<unsigned>(S()[9]) >> 1) + 0xB, 6, 9);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    if (s[9] > 5) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4FDEB0 (state 3): by +9 & 3: (3, 0x65DB4C[i]) to (3, 3), (7,
// 0x65DB5C[i]) to (6, 9), (7, 0x65DB5C[i] + 1) to (6, 0xA); +9 up.
extern "C" void __cdecl EffectKind18_1F_Cycle(void) {
    Copy(3, TL(::bof3::Img(at::kCycleA.v) + 4 * (S()[9] & 3u)), 3, 3);
    Copy(7, TL(::bof3::Img(at::kCycleB.v) + 4 * (S()[9] & 3u)), 6, 9);
    Copy(7, TL(::bof3::Img(at::kCycleB.v) + 4 * (S()[9] & 3u)) + 1, 6, 0xA);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
}

// original 0x4FDF20 (EffectKind18_States[6]).
extern "C" void __cdecl EffectKind18_06_Run(void) {
    Dispatch("EffectKind18_06_Run", AddressOf(EffectKind18_06_States), EffectKind18_06_States_count);
}

// original 0x4FDF40 (state 0): the word +0x34 the set's story flag, +0x3E the
// height of level +0x3A; +2 up one; the map, the draw.
extern "C" void __cdecl EffectKind18_06_Start(void) {
    static const char kWho[] = "EffectKind18_06_Start";
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_frac, T(::bof3::Img(at::kFlags6.v) + Index(kWho, static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), at::kSets6, at::kFlags6.v)));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, Word(At(::bof3::Img(at::kHeights6.v) + 2 * Index(kWho, static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole)), at::kHeightCount6, at::kHeights6.v))));
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18_06_SetMap)();
    SH_CALL(EffectKind18_06_Draw)();
}

// original 0x4FDF80 (state 1): the story flag +0x34 set: +9 0, +2 up one, the
// sounds 0x202 and 0x201; the draw.
extern "C" void __cdecl EffectKind18_06_Wait(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), S()[0x34]) != 0) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        SH_CALL(Sound_PlayEffect)(0x202);
        SH_CALL(Sound_PlayEffect)(0x201);
    }
    SH_CALL(EffectKind18_06_Draw)();
}

// original 0x4FDFD0 (state 2): +0x3E + 0x65DB90[+0x3A]; +9 up; at 0x10: the
// level (+0x3A + 1) & 3, the flag cleared, +2 down one; the map, the draw.
extern "C" void __cdecl EffectKind18_06_Turn(void) {
    static const char kWho[] = "EffectKind18_06_Turn";
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, rv::As<records::EffectObject>(s)->height_whole + static_cast<U>(TS(::bof3::Img(at::kSteps6.v) + Index(kWho, static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole)), at::kHeightCount6, at::kSteps6.v))));
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    s = S();
    if (s[9] >= 0x10) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, (s[0x3A] + 1u) & 3);
        SH_CALL(Flags_Clear)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), S()[0x34]);
        S()[2] = static_cast<unsigned char>(S()[2] - 1);
    }
    SH_CALL(EffectKind18_06_SetMap)();
    SH_CALL(EffectKind18_06_Draw)();
}

// original 0x4FE040 (cdecl): the set's rectangle 0x65DB6C[+0x36]: the map's
// bytes 0x21 (0 at the lowest level); then each cell's height byte from
// AreaMap_Elevation and +0x3E.
extern "C" void __cdecl EffectKind18_06_SetMap(void) {
    static const char kWho[] = "EffectKind18_06_SetMap";
    const auto rect = [](const unsigned char* s) {
        return ::bof3::Img(at::kRects.v) + 4u * static_cast<U>(Index(kWho, SW(s + 0x36), at::kSets6, at::kRects.v));
    };
    unsigned char* s = S();
    const unsigned char value = static_cast<std::uint16_t>(rv::As<records::EffectObject>(s)->height_whole) != at::kLowered ? 0x21 : 0;
    U r = rect(s);
    for (U x = T(r); x < T(r + 2); ++x) {
        for (U z = T(r + 1); z < T(r + 3);) {
            const U w = AreaMap_Header[0];
            unsigned char* const bytes = const_cast<unsigned char*>(AreaMap_Bytes.Native());
            bytes[w * z + x] = value;
            ++z;
            s = S();
            r = rect(s);
        }
        r = rect(s);
    }
    r = rect(s);
    for (U x = T(r); x < T(r + 2);) {
        for (U z = T(r + 1); z < T(r + 3);) {
            const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(x << 16), static_cast<long>(z << 16));
            s = S();
            std::int32_t v = -static_cast<std::int32_t>(static_cast<std::int16_t>(h)) - 2 * static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->height_whole));
            v = (v + ((v >> 31) & 0x1F)) >> 5;
            const U base = AreaMap_HeightBase;
            const U w = AreaMap_Header[0];
            *At(::bof3::Img(AreaMap_Header_addr) + 4 * base + x + w * z) = static_cast<unsigned char>(v);
            ++z;
            r = rect(s);
        }
        r = rect(s);
        ++x;
    }
}

// original 0x4FE040's tail 0x4FE1A0 (cdecl, its own frame): the set's parts
// 0x65DB9C[+0x36] (first, end, the end read again each pass), each a POLY_FT4
// of its shape row about its cell at the height +0x3E, linked at the cell.
extern "C" void __cdecl EffectKind18_06_Draw(void) {
    static const char kWho[] = "EffectKind18_06_Draw";
    const auto run = [](const unsigned char* s) {
        return ::bof3::Img(at::kRuns6.v) + 2u * static_cast<U>(Index(kWho, SW(s + 0x36), at::kSets6, at::kRuns6.v));
    };
    for (U i = T(run(S())); i < T(run(S()) + 1); ++i) {
        const U part = ::bof3::Img(at::kParts6.v) + 8 * i;
        unsigned char* const p = BeginQuad();
        const U w0 = TL(part);
        const U a = (static_cast<U>(T(part + 3)) << 7) - 0x3FC0;
        const U b = (static_cast<U>(T(part + 2)) << 7) - 0x3FC0;
        const U shape = ::bof3::Img(at::kShapes6.v) + 12 * ((w0 >> 8) & 0xFF);
        for (unsigned k = 0; k < 4; ++k) {
            const U vx = a - static_cast<U>(TS(shape + 3 * k) << 7);
            const U vy = b - static_cast<U>(TS(shape + 3 * k + 1) << 7);
            const U vz = static_cast<U>(TS(shape + 3 * k + 2) << 7) + static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->height_whole);
            Vertex(k, vx, vy, vz);
        }
        Project4(p);
        SH_CALL(Prim_SetTexture)(TL(part + 4), p, 1);
        const U raised = static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->height_whole) != at::kLowered ? 1 : 0;
        const U dy = T(::bof3::Img(at::kDy6.v) + (w0 & 0xFF) * 2 + raised);
        SH_CALL(MapView_LinkPrimAt)((w0 >> 8) & 0xFF0000, w0 & 0xFF0000, static_cast<int>(dy), 0x48);
    }
}

// original 0x4FE330 (EffectKind18_States[7]).
extern "C" void __cdecl EffectKind18_07_Run(void) {
    Dispatch("EffectKind18_07_Run", AddressOf(EffectKind18_07_States), EffectKind18_07_States_count);
}

// original 0x4FE350 (state 0): +0xB the door (+0x36); its flag set: +2 3, the
// row + 2; else +2 up one, the row, its two row-0x29 cells blocked (0x50);
// +0x38 (row << 7) - 0x4040, +0x34 -0x2B80; the draws.
extern "C" void __cdecl EffectKind18_07_Start(void) {
    static const char kWho[] = "EffectKind18_07_Start";
    S()[0xB] = S()[0x36];
    if (DoorFlag(Door(kWho, S()))) {
        S()[2] = 3;
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->z = T(::bof3::Img(at::kRows7.v) + Door(kWho, s)) + 2u;
    } else {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->z = T(::bof3::Img(at::kRows7.v) + Door(kWho, s));
        SH_CALL(AreaMap_SetByte)(0x29, T(::bof3::Img(at::kRows7.v) + Door(kWho, S())), 0x50);
        SH_CALL(AreaMap_SetByte)(0x29, T(::bof3::Img(at::kRows7.v) + Door(kWho, S())) + 1u, 0x50);
    }
    rv::Current<records::EffectObject>()->z = (static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) << 7) - 0x4040;
    rv::Current<records::EffectObject>()->x = 0xFFFFD480u;
    DrawDoor();
}

// original 0x4FE440 (state 1): the door's flag set: +9 0, +2 up one; the draws.
extern "C" void __cdecl EffectKind18_07_Wait(void) {
    if (DoorFlag(Door("EffectKind18_07_Wait", S()))) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    DrawDoor();
}

// original 0x4FE490 (state 2): +9 up, +0x38 + 8; at 0x20: +2 up one, the two
// cells freed (0); the panels only (a tail jump to EffectKind18_07_Draw).
extern "C" void __cdecl EffectKind18_07_Slide(void) {
    static const char kWho[] = "EffectKind18_07_Slide";
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    rv::Current<records::EffectObject>()->z = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) + 8;
    unsigned char* const s = S();
    if (s[9] >= 0x20) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        SH_CALL(AreaMap_SetByte)(0x29, T(::bof3::Img(at::kRows7.v) + Door(kWho, S())), 0);
        SH_CALL(AreaMap_SetByte)(0x29, T(::bof3::Img(at::kRows7.v) + Door(kWho, S())) + 1u, 0);
    }
    SH_CALL(EffectKind18_07_Draw)();
}

// original 0x4FE510 (state 3, and the panels' draw): two POLY_FT4 stacked 0x80
// apart from +0x38, at x +0x34, z -0x200 / -0x80; linked one above.
extern "C" void __cdecl EffectKind18_07_Draw(void) {
    for (U i = 0; i < 2; ++i) {
        unsigned char* const p = BeginQuad();
        const unsigned char* s = S();
        const U x = rv::As<records::EffectObject>(s)->x_frac, y = rv::As<records::EffectObject>(s)->z_frac;
        Vertex(0, x, ((i + 1) << 7) + y, 0xFFFFFE00u);
        Vertex(1, x, (i << 7) + y, 0xFFFFFE00u);
        Vertex(2, x, ((i + 1) << 7) + y, 0xFFFFFF80u);
        Vertex(3, x, (i << 7) + y, 0xFFFFFF80u);
        Project4(p);
        SH_CALL(Prim_SetTexture)(TL(::bof3::Img(at::kTextures7.v) + 4 * i), p, 1);
        s = S();
        SH_CALL(MapView_LinkPrimAt)((static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0x4000) << 9, (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + 0x80 * i + 0x4040) << 9, 1, 0x48);
    }
}

// original 0x4FE640 (cdecl, its own frame): the top: one POLY_FT4 at x +0x34
// + 0x10, +0x38 + 0x80 .. + 0x100, z -0x160 / -0xE0.
extern "C" void __cdecl EffectKind18_07_DrawTop(void) {
    unsigned char* const p = BeginQuad();
    const unsigned char* s = S();
    const U x = Word(s + 0x34) + 0x10u, y = Word(s + 0x38);
    Vertex(0, x, y + 0x100, 0xFFFFFEA0u);
    Vertex(1, x, y + 0x80, 0xFFFFFEA0u);
    Vertex(2, x, y + 0x100, 0xFFFFFF20u);
    Vertex(3, x, y + 0x80, 0xFFFFFF20u);
    Project4(p);
    SH_CALL(Prim_SetTexture)(0xBD5000F9u, p, 1);
    s = S();
    SH_CALL(MapView_LinkPrimAt)((static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0x4000) << 9, (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + 0x40C0) << 9, 1, 0x48);
}

// original 0x4FE750 (EffectKind18_States[8]).
extern "C" void __cdecl EffectKind18_08_Run(void) {
    Dispatch("EffectKind18_08_Run", AddressOf(EffectKind18_08_States), EffectKind18_08_States_count);
}

// original 0x4FE770 (state 0): flag 6 of Cond_Flags row 3 set: +2 6, +0x34
// -0x2940; else +2 up one, +0x34 -0x2B80; +0x38 -0x31C0, +0x3E -0x80; the draws.
extern "C" void __cdecl EffectKind18_08_Start(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x18), 6) != 0) {
        S()[2] = 6;
        rv::Current<records::EffectObject>()->x = 0xFFFFD6C0u;
    } else {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        rv::Current<records::EffectObject>()->x = 0xFFFFD480u;
    }
    rv::Current<records::EffectObject>()->z = 0xFFFFCE40u;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0xFF80);
    DrawDoor();
}

// original 0x4FE7E0 (state 1): flag 5 of row 3 set: +9 0, +2 up one; the draws.
extern "C" void __cdecl EffectKind18_08_Wait(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x18), 5) != 0) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    DrawDoor();
}

// original 0x4FE810 (state 2): +9 up, +0x34 (+9 & 1) * 8 - 0x2B80; at 8: +9 0,
// +0x34 -0x2B80, +2 up one; the draws.
extern "C" void __cdecl EffectKind18_08_Shake(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* s = S();
    rv::As<records::EffectObject>(s)->x = (s[9] & 1u) * 8 - 0x2B80u;
    s = S();
    if (s[9] >= 8) {
        s[9] = 0;
        rv::Current<records::EffectObject>()->x = 0xFFFFD480u;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    DrawDoor();
}

// original 0x4FE860 (state 3): flag 6 of row 3 set: +0x34 + 0x40, +9 0, +2 up
// one; the draws.
extern "C" void __cdecl EffectKind18_08_WaitPush(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x18), 6) != 0) {
        rv::Current<records::EffectObject>()->x = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x) + 0x40;
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    DrawDoor();
}

// original 0x4FE8A0 (state 4): the panels tilted by 0x65DDF0[+9] << 4; +9 up;
// past 0x28: the sound 0x205, +9 0, +0x3E -0x88, +2 up one.
extern "C" void __cdecl EffectKind18_08_Topple(void) {
    const unsigned n = static_cast<unsigned>(Index("EffectKind18_08_Topple", S()[9], at::kTopple8Count, at::kTopple8.v));
    SH_CALL(EffectKind18_08_DrawTilted)(Div256(T(::bof3::Img(at::kTopple8.v) + n)));
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= 0x28) return;
    SH_CALL(Sound_PlayEffect)(0x205);
    S()[9] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0xFF78);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FE920 (state 5): the tilt 0x65DE1C[+9] (or, at 0, the fallen
// tiles); seven puffs spread by +9 about the record's point, each a POLY_FT4
// of +9 either side of its projected point; +9 up; past 0xD: +2 up one.
extern "C" void __cdecl EffectKind18_08_Crash(void) {
    const unsigned n0 = static_cast<unsigned>(Index("EffectKind18_08_Crash", S()[9], at::kCrash8Count, at::kCrash8.v));
    const unsigned tilt = T(::bof3::Img(at::kCrash8.v) + n0);
    if (tilt != 0)
        SH_CALL(EffectKind18_08_DrawTilted)(Div256(tilt));
    else
        SH_CALL(EffectKind18_08_DrawFallen)();
    for (U j = 0; j < 0xE; j += 2) {
        unsigned char* const p = BeginQuad();
        const unsigned char* s = S();
        const U n = s[9];
        const U vx = ((static_cast<U>(TS(::bof3::Img(at::kPuffStep.v) + j)) * n + (static_cast<U>(T(::bof3::Img(at::kPuffBase.v) + j)) << 4)) << 3) + Word(s + 0x34);
        const U vy = ((static_cast<U>(TS(::bof3::Img(at::kPuffStep.v) + j + 1)) * n + (static_cast<U>(T(::bof3::Img(at::kPuffBase.v) + j + 1)) << 4)) << 3) + Word(s + 0x38);
        Prim_VertexScratch[0].x = vx & 0xFFFF;
        Prim_VertexScratch[0].y = vy & 0xFFFF;
        Prim_VertexScratch[0].z = Word(s + 0x3E);
        long depth;
        SH_CALL(Gte_RotTransPers)(reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<unsigned long*>(At(::bof3::Img(0x903820))), &depth);
        SH_CALL(Gte_PrimDepthFlat4_10)(p);
        FildSubr2(S()[9], At(::bof3::Img(0x903820)), p + 0x28, p + 8);
        FildSubr2(S()[9], At(::bof3::Img(0x903824)), p + 0x1C, p + 0xC);
        FildAdd2(S()[9], At(::bof3::Img(0x903820)), p + 0x38, p + 0x18);
        FildAdd2(S()[9], At(::bof3::Img(0x903824)), p + 0x3C, p + 0x2C);
        SH_CALL(Prim_SetTexture)(0xBA689124u - (static_cast<U>(S()[9]) << 19), p, 1);
        s = S();
        SH_CALL(MapView_LinkPrimAt)((UL(s + 0x34) + 0x4000) << 9, (UL(s + 0x38) + 0x4000) << 9, 6, 0x48);
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    if (s[9] > 0xD) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4FEAF0 (state 6, and a draw): six floor tiles at the cells of
// 0x65DE4C, z -0x88, linked at each.
extern "C" void __cdecl EffectKind18_08_DrawFallen(void) {
    for (U k = 0; k < 6; ++k) {
        unsigned char* const p = BeginQuad();
        const U tx = T(::bof3::Img(at::kTiles8.v) + 2 * k), tz = T(::bof3::Img(at::kTiles8.v) + 2 * k + 1);
        const U x = tx << 7, z = tz << 7;
        Vertex(0, x - 0x3FC0, z - 0x3FC0, 0xFFFFFF78u);
        Vertex(1, x - 0x3FC0, z - 0x4040, 0xFFFFFF78u);
        Vertex(2, x - 0x4040, z - 0x3FC0, 0xFFFFFF78u);
        Vertex(3, x - 0x4040, z - 0x4040, 0xFFFFFF78u);
        Project4(p);
        SH_CALL(Prim_SetTexture)(static_cast<U>(T(::bof3::Img(at::kTileTex8.v) + k)) - 0x42B00000u, p, 1);
        SH_CALL(MapView_LinkPrimAt)(tx << 16, tz << 16, 0, 0x48);
    }
}

// original 0x4FEC20 (cdecl, one word): the two panels turned by -angle about y
// at the record's point; linked two above.
extern "C" void __cdecl EffectKind18_08_DrawTilted(unsigned angle) {
    const unsigned char* s = S();
    short point[4];
    point[0] = static_cast<short>(rv::As<records::EffectObject>(s)->x_frac);
    point[1] = static_cast<short>(rv::As<records::EffectObject>(s)->z_frac);
    point[2] = static_cast<short>(rv::As<records::EffectObject>(s)->height_whole);
    short angles[4];
    angles[0] = 0;
    angles[1] = static_cast<short>(0u - angle);
    angles[2] = 0;
    PushMatrix(point, angles);
    U counter = 0;
    for (U i = 0; i < 2; ++i) {
        unsigned char* const p = BeginQuad();
        Vertex(0, 0, (i + 1) << 7, 0xFFFFFE80u);
        Vertex(1, 0, i << 7, 0xFFFFFE80u);
        Vertex(2, 0, (i + 1) << 7, 0);
        Vertex(3, 0, i << 7, 0);
        Project4(p);
        SH_CALL(Prim_SetTexture)(TL(::bof3::Img(at::kTextures7.v) + 4 * i), p, 1);
        s = S();
        SH_CALL(MapView_LinkPrimAt)((static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0x4000) << 9, (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + counter + 0x4040) << 9, 2, 0x48);
        counter += 0x80;
    }
    SH_CALL(Gte_PopMatrix)();
}

void EffectKind18_Inject() {
    BOF3_INJECT(EffectKind18_04_Run);
    BOF3_INJECT(EffectKind18_04_Start);
    BOF3_INJECT(EffectKind18_04_Shut);
    BOF3_INJECT(EffectKind18_04_Swing);
    BOF3_INJECT(EffectKind18_04_Open);
    BOF3_INJECT(EffectKind18_04_DrawPanel);
    BOF3_INJECT(EffectKind18_05_Run);
    BOF3_INJECT(EffectKind18_05_Start);
    BOF3_INJECT(EffectKind18_05_Idle);
    BOF3_INJECT(EffectKind18_05_FadeIn);
    BOF3_INJECT(EffectKind18_05_Pulse);
    BOF3_INJECT(EffectKind18_05_FadeOut);
    BOF3_INJECT(Gfx_ClutStripCopy16);
    BOF3_INJECT(EffectKind18_1A_Run);
    BOF3_INJECT(EffectKind18_1A_Start);
    BOF3_INJECT(EffectKind18_1A_WaitOn);
    BOF3_INJECT(EffectKind18_1A_SlideOut);
    BOF3_INJECT(EffectKind18_1A_WaitOff);
    BOF3_INJECT(EffectKind18_1A_SlideBack);
    BOF3_INJECT(EffectKind18_1A_Draw);
    BOF3_INJECT(EffectKind18_1F_Run);
    BOF3_INJECT(EffectKind18_1F_Start);
    BOF3_INJECT(EffectKind18_1F_Wait);
    BOF3_INJECT(EffectKind18_1F_Brighten);
    BOF3_INJECT(EffectKind18_1F_Cycle);
    BOF3_INJECT(EffectKind18_06_Run);
    BOF3_INJECT(EffectKind18_06_Start);
    BOF3_INJECT(EffectKind18_06_Wait);
    BOF3_INJECT(EffectKind18_06_Turn);
    BOF3_INJECT(EffectKind18_06_SetMap);
    BOF3_INJECT(EffectKind18_06_Draw);
    BOF3_INJECT(EffectKind18_07_Run);
    BOF3_INJECT(EffectKind18_07_Start);
    BOF3_INJECT(EffectKind18_07_Wait);
    BOF3_INJECT(EffectKind18_07_Slide);
    BOF3_INJECT(EffectKind18_07_Draw);
    BOF3_INJECT(EffectKind18_07_DrawTop);
    BOF3_INJECT(EffectKind18_08_Run);
    BOF3_INJECT(EffectKind18_08_Start);
    BOF3_INJECT(EffectKind18_08_Wait);
    BOF3_INJECT(EffectKind18_08_Shake);
    BOF3_INJECT(EffectKind18_08_WaitPush);
    BOF3_INJECT(EffectKind18_08_Topple);
    BOF3_INJECT(EffectKind18_08_Crash);
    BOF3_INJECT(EffectKind18_08_DrawFallen);
    BOF3_INJECT(EffectKind18_08_DrawTilted);
}
