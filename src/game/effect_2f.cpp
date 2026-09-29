// Effect kinds 0x4F, 0x50, 0x51, 0x52, 0x53, 0x56 and kind 0x4E's disc - round
// thirteen, wave two, group E2F: the 52 functions of the cut
// (analysis/round13_cut.tsv) in 0x47B7D0..0x47DBD1, the ten more of the band
// that are states or callees of the same kinds (0x47C0F0, 0x47C190, 0x47C320,
// 0x47C4A0, 0x47CF20, 0x47CF40, 0x47CFC0, 0x47CFF0, 0x47D010, 0x47D8B0), each
// read with capstone to its last instruction (docs/effect_2f.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x4E  (E2E's) draws the disc here, EffectKind4E_DrawDisc;
//   kind 0x4F  two sprites tinted and set apart (EffectKind3C_Start's twin with
//              its own event op), then FC1's EffectKind3C_Hold and E2D's state;
//   kind 0x50  specks thrown from the record's point in a turning direction,
//              64 of 0x28 bytes at EffectKind30_Shards, each a textured quad;
//   kind 0x51  a column of quads swaying on a history of 65 angles, which
//              grows, waits on the counter byte 0x903848, shrinks, then a ring
//              of sixteen quad pairs closing or opening;
//   kind 0x52  sparks (8 of 0x1C at EffectKind30_Shards) rising round the
//              record, then a trail of 32 points at 0x92C060 that the record
//              drags along two fixed paths, drawn as Gouraud quads with caps;
//   kind 0x53  a textured beam from the top of the screen down onto an extra
//              sprite, between two texture-window primitives;
//   kind 0x56  a column of red crosses from Sprite_ObjectsExtra[0]'s height
//              down to the ground, armed and stopped by the counter byte
//              0x90384B.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes past a pool or a table, or loops for ever, ours
// aborts with a message (docs/effect_2f.md section 2). The x87 arithmetic is
// done in long double, which the compiler keeps on the x87 at the control
// word's precision, as the originals' fld / fadd / fsub / fstp chains are.
#include "game/effect_2f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2f::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }
U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul then sar 12: a trigonometric answer scaled by a length.
U Scale(int trig, U length) { return Sar(static_cast<U>(trig) * length, 12); }
// (v << 5) sar 12: a trigonometric answer to -32..32, a shade's offset.
unsigned char Shade(int trig) { return static_cast<unsigned char>(Sar(static_cast<U>(trig) << 5, 12)); }

// --- x87 as the original has it -------------------------------------------------
// `fld dword [p]`: the float widened (a signalling NaN comes out quiet, as fld
// makes it).
LD X(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return static_cast<LD>(f);
}
// `fild dword`: an int widened, exactly.
LD XI(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }
// `fstp dword [p]`: rounded to a float at the control word's rounding.
void Fstp(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// `fild dword [v]; fstp dword [t]`: an int as a float, held in a float local.
LD AsFloat(U v) {
    const float f = static_cast<float>(XI(v));
    return static_cast<LD>(f);
}
// `fld dword [src]; fstp dword [dst]`: a copy through the FPU (a signalling NaN
// quietened), kept in assembly so the compiler cannot fold it into a move.
void FCopy(unsigned char* dst, const unsigned char* src) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(dst), "r"(src) : "st", "memory");
}
// `fcomp` then `test ah, 0x40`: C3, equal or unordered.
bool EqualOrUnordered(LD a, LD b) { return !(a < b) && !(a > b); }
// The CRT's _ftol 0x5B9550 on st(0): truncation to 64 bits in a copy of the
// control word, the low dword kept; NaN and out-of-range answer the integer
// indefinite 0x8000000000000000, whose low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775809.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// --- the tables and the pools ------------------------------------------------------

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Jump(const char* who, U table, unsigned entries, unsigned index) {
    if (index >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2f.md section 2)",
                    who, index, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * index))))();
}

// A Sprite_Objects record by the index a state keeps: a Fatal past the thirty
// (Sprite_FindFree answers 0..29 or 0xFF; the original would write past the pool).
unsigned char* Sprite(unsigned index, const char* who) {
    if (index >= at::kSprites)
        bof3::Fatal("%s: Sprite_Objects index %u, past the 30 records - the original writes past the pool "
                    "(docs/effect_2f.md section 2)",
                    who, index);
    return Sprite_Objects + index * at::kSpriteStride;
}
// A Sprite_ObjectsExtra record by the dword +0x18: a Fatal past the four.
unsigned char* Extra(U index, const char* who) {
    if (index >= at::kExtras)
        bof3::Fatal("%s: +0x18 is %u, past the 4 records of Sprite_ObjectsExtra - the original reads past them "
                    "(docs/effect_2f.md section 2)",
                    who, (unsigned)index);
    return Sprite_ObjectsExtra + index * at::kSpriteStride;
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}
// +2 up, Sprite_Current read afresh.
void NextSub() {
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
// The word at p down one.
void DecWord(unsigned char* p) { SetWord(p, Word(p) - 1u); }
void IncWord(unsigned char* p) { SetWord(p, Word(p) + 1u); }

// Gpu_GetTPage(tp, abr, x, y), then the draw mode of that page at the packet
// cursor (dfe 0, dtd as given), committed to slot `slot` (0xC bytes).
void DrawMode(unsigned tp, unsigned abr, int x, int y, int dtd, unsigned slot) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(tp, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}  // namespace

// ===========================================================================
// Kind 0x4E's disc (E2E owns the kind; its states 1..3 call this)
// ===========================================================================

// original 0x47B7D0 (0x19D bytes): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1, committed 0xC); then for a = 0, 0x80, .. 0xF80 (b = a + 0x80 & 0xFFF) a
// Gouraud triangle at the packet cursor (read afresh): Gpu_SetPolyG3,
// Gpu_SetSemiTrans(1), Gte_StoreDepthF(prim + 0x10) copied to +0x30 and +0x20;
// vertex 0 the record's screen point (+0x74, +0x78, copied as they are); vertex 1
// its x + (cos(a) r sar 12), its y + (sin(a) r sar 12) (floats, fild + fadd);
// vertex 2 likewise at b; the depth +0x7C to +0x30, +0x20, +0x10; vertex 0
// shaded `centre`, 1 and 2 `rim`; committed 0x34. Sprite_Current is read afresh
// after every call, as the original reads it. r is read as s16, the shades as
// bytes (the callers push whole registers).
extern "C" void __cdecl EffectKind4E_DrawDisc(unsigned radius, unsigned centre, unsigned rim) {
    DrawMode(0, 1, 0x3C0, 0, 1, 1);
    const U r = S16(radius);
    const auto rim_shade = static_cast<unsigned char>(rim);
    const auto centre_shade = static_cast<unsigned char>(centre);
    for (U a = 0; a < 0x1000; a += 0x80) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
        FCopy(prim + 0x30, prim + 0x10);
        FCopy(prim + 0x20, prim + 0x10);
        const U b = (a + 0x80) & 0xFFFu;
        std::memcpy(prim + 8, S() + 0x74, 4);
        std::memcpy(prim + 0xC, S() + 0x78, 4);
        U v = Scale(SH_CALL(Math_Cos)(static_cast<int>(a)), r);
        Fstp(prim + 0x18, XI(v) + X(S() + 0x74));
        v = Scale(SH_CALL(Math_Sin)(static_cast<int>(a)), r);
        Fstp(prim + 0x1C, XI(v) + X(S() + 0x78));
        v = Scale(SH_CALL(Math_Cos)(static_cast<int>(b)), r);
        Fstp(prim + 0x28, XI(v) + X(S() + 0x74));
        v = Scale(SH_CALL(Math_Sin)(static_cast<int>(b)), r);
        Fstp(prim + 0x2C, XI(v) + X(S() + 0x78));
        const unsigned char* const s = S();
        prim[4] = centre_shade;
        prim[5] = centre_shade;
        FCopy(prim + 0x30, s + 0x7C);
        FCopy(prim + 0x20, s + 0x7C);
        FCopy(prim + 0x10, s + 0x7C);
        prim[6] = centre_shade;
        prim[0x14] = prim[0x15] = prim[0x16] = rim_shade;
        prim[0x24] = prim[0x25] = prim[0x26] = rim_shade;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// ===========================================================================
// Kind 0x4F: Effect_KindHandlers[0x4F] (0x65548C), EffectKind4F_States (three)
// ===========================================================================

// original 0x47B970 (hidden in 0x47B7D0's recorded extent): jmp
// [EffectKind4F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4F_Run(void) {
    Jump("EffectKind4F_Run", AddressOf(EffectKind4F_States), EffectKind4F_States_count, S()[1]);
}

// original 0x47B990 (EffectKind4F_States[0]): FC1's EffectKind3C_Start with the
// op EffectKind4F_Op. Two free sprites into +3 and +4 (none: nothing, the first
// given back when only the second fails), each marked in use; for each,
// DamageScratch's word = its index and EventOp_0x(EffectKind4F_Op); Sprite_Current
// put back; the first moved by (-0x1000, +0x1000), the second by (+0x1000,
// -0x1000), both +0 |= 0x20, +0x5C = 0, tinted (0xF, 0, 0, 1) and (0, 0, 0xF, 1);
// +9 = 4 and +1 = 1 (Sprite_Current read again after the tints).
extern "C" void __cdecl EffectKind4F_Start(void) {
    const char* const who = "EffectKind4F_Start";
    S()[3] = SH_CALL(Sprite_FindFree)();
    if (S()[3] == 0xFF) return;
    Sprite(S()[3], who)[0] = 1;
    S()[4] = SH_CALL(Sprite_FindFree)();
    unsigned char* const self = S();
    if (self[4] == 0xFF) {
        Sprite(self[3], who)[0] = 0;
        return;
    }
    Sprite(self[4], who)[0] = 1;
    SetWord(At(bof3::addr::DamageScratch), self[3]);
    SH_CALL(EventOp_0x)(EffectKind4F_Op);
    SetWord(At(bof3::addr::DamageScratch), self[4]);
    SH_CALL(EventOp_0x)(EffectKind4F_Op);
    Sprite_Current = self;
    unsigned char* a = Sprite(self[3], who);
    SetUL(a + 0x34, UL(a + 0x34) + 0xFFFFF000u);
    a = Sprite(self[3], who);
    SetUL(a + 0x38, UL(a + 0x38) + 0x1000u);
    unsigned char* b = Sprite(self[4], who);
    SetUL(b + 0x34, UL(b + 0x34) + 0x1000u);
    b = Sprite(self[4], who);
    SetUL(b + 0x38, UL(b + 0x38) + 0xFFFFF000u);
    a = Sprite(self[3], who);
    a[0] = static_cast<unsigned char>(a[0] | 0x20);
    b = Sprite(self[4], who);
    b[0] = static_cast<unsigned char>(b[0] | 0x20);
    Sprite(self[4], who)[0x5C] = 0;
    Sprite(self[3], who)[0x5C] = 0;
    SH_CALL(Sprite_SetTint)(Sprite(self[3], who), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(Sprite(self[4], who), 0, 0, 0xF, 1);
    S()[9] = 4;
    S()[1] = 1;
}

// ===========================================================================
// Kind 0x50: Effect_KindHandlers[0x50] (0x655490), EffectKind50_States (three)
// ===========================================================================

// original 0x47BB70: jmp [EffectKind50_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind50_Run(void) {
    Jump("EffectKind50_Run", AddressOf(EffectKind50_States), EffectKind50_States_count, S()[1]);
}

// original 0x47BB90 (state 0): the specks cleared, the direction dword +0xC =
// 0, +1 up.
extern "C" void __cdecl EffectKind50_Start(void) {
    SH_CALL(EffectKind50_ClearSpecks)();
    SetUL(S() + 0xC, 0);
    NextState();
}

// original 0x47BBB0 (state 1): one frame in four (Frame_Counter's low two bits
// 0) a speck spawned; the specks moved and drawn; the direction +0xC turned
// 0x80 (& 0xFFF); +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind50_Emit(void) {
    if ((Frame_Counter & 3u) == 0) SH_CALL(EffectKind50_SpawnSpeck)();
    SH_CALL(EffectKind50_MoveSpecks)();
    unsigned char* const s = S();
    SetUL(s + 0xC, (UL(s + 0xC) + 0x80u) & 0xFFFu);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x47BC00 (state 2): the specks moved and drawn; once none was in
// use, a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind50_Drain(void) {
    if (SH_CALL(EffectKind50_MoveSpecks)() != 0) SH_CALL(Effect_Release)();
}

// original 0x47C350: +0 of each of the 64 specks (0x28 apart from
// EffectKind30_Shards) = 0.
extern "C" void __cdecl EffectKind50_ClearSpecks(void) {
    for (unsigned i = 0; i < at::kSpecks; ++i) EffectKind30_Shards[i * at::kSpeckStride] = 0;
}

// original 0x47C370 (0x12C bytes with its switch's four cases; 0x47C420, the
// cut's NOTFN row, is case 3): a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100),
// dtd 0, committed 0xC), EffectGte_LoadMapCamera; then each speck in use (+0) by
// its phase +2 - 0: the count +1 = 8, the shade +3 = 0, the size +0x24 = 0,
// +2 up; 1: the shade up 8, at count 0 the count 0x10 and +2 up; 2: at count 0
// the count 8 and +2 up; 3: the shade down 8, at count 0 +0 = 0 (freed, still
// drawn and moved this frame); past 3 nothing - drawn (EffectKind50_SpeckQuad),
// its point +4 / +8 / +0xC moved by +0x14 / +0x18 / +0x1C, the fall +0x1C and
// the size +0x24 up 0x200, the count +1 down. Answers 1 when no speck was in
// use, else 0.
extern "C" unsigned char __cdecl EffectKind50_MoveSpecks(void) {
    DrawMode(0, 1, 0x2C0, 0x100, 0, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char none = 1;
    for (unsigned i = 0; i < at::kSpecks; ++i) {
        unsigned char* const k = EffectKind30_Shards + i * at::kSpeckStride;
        if (k[0] == 0) continue;
        none = 0;
        switch (k[2]) {
        case 0:
            k[1] = 8;
            k[3] = 0;
            SetUL(k + 0x24, 0);
            k[2] = static_cast<unsigned char>(k[2] + 1);
            break;
        case 1:
            k[3] = static_cast<unsigned char>(k[3] + 8);
            if (k[1] == 0) {
                k[1] = 0x10;
                k[2] = static_cast<unsigned char>(k[2] + 1);
            }
            break;
        case 2:
            if (k[1] == 0) {
                k[1] = 8;
                k[2] = static_cast<unsigned char>(k[2] + 1);
            }
            break;
        case 3:
            k[3] = static_cast<unsigned char>(k[3] - 8);
            if (k[1] == 0) k[0] = 0;
            break;
        default: break;
        }
        SH_CALL(EffectKind50_SpeckQuad)(k);
        SetUL(k + 4, UL(k + 4) + UL(k + 0x14));
        SetUL(k + 8, UL(k + 8) + UL(k + 0x18));
        const U fall = UL(k + 0x1C);
        SetUL(k + 0xC, UL(k + 0xC) + fall);
        SetUL(k + 0x1C, fall + 0x200u);
        SetUL(k + 0x24, UL(k + 0x24) + 0x200u);
        k[1] = static_cast<unsigned char>(k[1] - 1);
    }
    return none;
}

// original 0x47C4A0 (0x14E bytes; in no list: the tool found it in 0x47C370's
// span): a speck's POLY_FT4 at the packet cursor (read once), semi-transparent.
// Its point (x +4, z +8, height +0xC << 8) projected (EffectGte_ProjectPoint)
// and its size (+0x24 shr 6, a word) scaled at that depth (EffectGte_ProjectSize:
// the original hands a size whose second word it never wrote, and reads only
// the first answer, w); the corners x - w and x + w, y - w and y + w (fild w,
// fsubr / fadd); the depth copied to +0x10 +0x20 +0x30 +0x40; the CLUT
// Gpu_GetClut(0xA0, 0x1E3), the page Gpu_GetTPage(0, 1, 0x2C0, 0x100); u 0xE0 /
// 0xFF, v 0x30 / 0x4F; red the speck's shade +3, green and blue 0; committed
// 0x48 in slot 1.
extern "C" void __cdecl EffectKind50_SpeckQuad(unsigned char* speck) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    short size[2] = {static_cast<short>(UL(speck + 0x24) >> 6), 0};
    const long point[4] = {Long(speck + 4), Long(speck + 8), static_cast<long>(UL(speck + 0xC) << 8), Long(speck + 0x10)};
    short out[2] = {0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, size, out);
    unsigned char screen[12];
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(screen));
    const U w = S16(static_cast<U>(static_cast<unsigned short>(out[0])));
    LD t = X(screen) - XI(w);
    Fstp(prim + 0x28, t);
    Fstp(prim + 8, t);
    t = XI(w) + X(screen);
    Fstp(prim + 0x38, t);
    Fstp(prim + 0x18, t);
    t = X(screen + 4) - XI(w);
    Fstp(prim + 0x1C, t);
    Fstp(prim + 0xC, t);
    t = XI(w) + X(screen + 4);
    Fstp(prim + 0x3C, t);
    Fstp(prim + 0x2C, t);
    std::memcpy(prim + 0x40, screen + 8, 4);
    std::memcpy(prim + 0x30, screen + 8, 4);
    std::memcpy(prim + 0x20, screen + 8, 4);
    std::memcpy(prim + 0x10, screen + 8, 4);
    SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    prim[0x14] = 0xE0;
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    prim[4] = speck[3];
    prim[5] = 0;
    prim[6] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x47C5F0: a free speck (none: nothing) put in use (+0 = 1, phase +2
// = 0) at the record's point (+0x34, +0x38, +0x3C sar 8); a = sin(the direction
// dword +0xC) << 8 sar 12, and with e = (a & 0xFFFF) + 0x800 when the record's
// +6 is 7 (read after that call), else + 0x400: its step +0x14 = 3 cos(e), +0x18
// = 3 sin(e), its fall +0x1C = 0.
extern "C" void __cdecl EffectKind50_SpawnSpeck(void) {
    unsigned char* const k = SH_CALL(EffectKind50_FreeSpeck)();
    if (k == nullptr) return;
    k[0] = 1;
    k[2] = 0;
    SetUL(k + 4, UL(S() + 0x34));
    SetUL(k + 8, UL(S() + 0x38));
    SetUL(k + 0xC, Sar(UL(S() + 0x3C), 8));
    const U a = Sar(static_cast<U>(SH_CALL(Math_Sin)(I(UL(S() + 0xC)))) << 8, 12);
    const U e = (a & 0xFFFFu) + (S()[6] == 7 ? 0x800u : 0x400u);
    SetUL(k + 0x14, static_cast<U>(SH_CALL(Math_Cos)(I(e))) * 3u);
    SetUL(k + 0x18, static_cast<U>(SH_CALL(Math_Sin)(I(e))) * 3u);
    SetUL(k + 0x1C, 0);
}

// original 0x47C6C0: the first of the 64 specks whose +0 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind50_FreeSpeck(void) {
    for (unsigned i = 0; i < at::kSpecks; ++i) {
        unsigned char* const k = EffectKind30_Shards + i * at::kSpeckStride;
        if (k[0] == 0) return k;
    }
    return nullptr;
}

// ===========================================================================
// Kind 0x51: Effect_KindHandlers[0x51] (0x655494), EffectKind51_States (twelve)
// ===========================================================================

namespace {

// The sway word +0x58 up by Rand's low byte - the word's address taken before
// the call, as the original's `lea esi, [eax + 0x58]` takes it.
void SwayStep() {
    unsigned char* const sway = S() + 0x58;
    const U r = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
    SetWord(sway, Word(sway) + r);
}

// States 0 and 6: the timer +0x5A = 0x20, the column's half-height +0x32 = 0,
// the sway +0x58 = 0; 0x40 times the sway stepped and pushed into
// EffectKind51_Angles (the column starts on a random history); the sway's
// width +0x2C = 0, the sound, +1 up.
void Kind51Start(unsigned short sound) {
    SetWord(S() + 0x5A, 0x20);
    SetWord(S() + 0x32, 0);
    SetWord(S() + 0x58, 0);
    for (unsigned i = 0; i < 0x40; ++i) {
        SwayStep();
        SH_CALL(EffectKind51_PushAngle)(Word(S() + 0x58));
    }
    SetWord(S() + 0x2C, 0);
    SH_CALL(Sound_PlayEffect)(sound);
    NextState();
}

// States 1 and 7: the column drawn, the sway stepped; when the timer's low two
// bits are 0 the width +0x2C up one; the half-height +0x32 up one; the timer
// down, at 0 the timer = `reload` and +1 up.
void Kind51Grow(unsigned reload) {
    SH_CALL(EffectKind51_DrawFrame)();
    SwayStep();
    unsigned char* const s = S();
    if ((s[0x5A] & 3) == 0) IncWord(s + 0x2C);
    IncWord(s + 0x32);
    DecWord(s + 0x5A);
    if (Word(s + 0x5A) != 0) return;
    SetWord(s + 0x5A, reload);
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// States 2 and 8: the column drawn, the sway stepped; once the counter byte
// 0x903848 is 0xB, the timer = 0x20 and +1 up.
void Kind51Wait() {
    SH_CALL(EffectKind51_DrawFrame)();
    SwayStep();
    if (At(at::kCounter0)[0] != 0xB) return;
    SetWord(S() + 0x5A, 0x20);
    NextState();
}

// The ring at the half-height +0x32 (its word as the original pushes it).
void Ring() { SH_CALL(EffectKind51_DrawRing)(Word(S() + 0x32)); }

}  // namespace

// original 0x47BC10 (hidden in 0x47B7D0's recorded extent): jmp
// [EffectKind51_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind51_Run(void) {
    Jump("EffectKind51_Run", AddressOf(EffectKind51_States), EffectKind51_States_count, S()[1]);
}

// original 0x47BC30 (state 0): Kind51Start with Sound_PlayEffect(0x208).
extern "C" void __cdecl EffectKind51_Start(void) { Kind51Start(0x208); }

// original 0x47BCB0 (state 1): Kind51Grow, the timer reloaded 0x96.
extern "C" void __cdecl EffectKind51_Grow(void) { Kind51Grow(0x96); }

// original 0x47BD10 (state 2): Kind51Wait.
extern "C" void __cdecl EffectKind51_Wait(void) { Kind51Wait(); }

// original 0x47BD50 (state 3): the column drawn, the sway stepped; when the
// timer's low two bits are 0 the width +0x2C down one; at timer 2
// Sound_PlayEffect(0x20C) (Sprite_Current read after it); the timer down, at 0
// the timer = 0x40 and +1 up.
extern "C" void __cdecl EffectKind51_Shrink(void) {
    SH_CALL(EffectKind51_DrawFrame)();
    SwayStep();
    unsigned char* s = S();
    if ((s[0x5A] & 3) == 0) DecWord(s + 0x2C);
    if (Word(s + 0x5A) == 2) {
        SH_CALL(Sound_PlayEffect)(0x20C);
        s = S();
    }
    DecWord(s + 0x5A);
    s = S();
    if (Word(s + 0x5A) != 0) return;
    SetWord(s + 0x5A, 0x40);
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x47BDC0 (state 4): the ring; when the timer's low two bits are 0
// the half-height +0x32 (the ring's radius) down one; the timer down, at 0
// Sound_PlayEffect(0x209), the timer = 0x14 and +1 up.
extern "C" void __cdecl EffectKind51_RingIn(void) {
    Ring();
    unsigned char* s = S();
    if ((s[0x5A] & 3) == 0) DecWord(s + 0x32);
    DecWord(s + 0x5A);
    if (Word(S() + 0x5A) != 0) return;
    SH_CALL(Sound_PlayEffect)(0x209);
    s = S();
    SetWord(s + 0x5A, 0x14);
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x47BE20 (state 5): the ring; the radius +0x32 up 0x10; the timer
// down, at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind51_RingOut(void) {
    Ring();
    unsigned char* const s = S();
    SetWord(s + 0x32, Word(s + 0x32) + 0x10u);
    DecWord(s + 0x5A);
    if (Word(S() + 0x5A) == 0) SH_CALL(Effect_Release)();
}

// original 0x47BE60 (state 6): Kind51Start with Sound_PlayEffect(0x202).
extern "C" void __cdecl EffectKind51_Start2(void) { Kind51Start(0x202); }

// original 0x47BEE0 (state 7): Kind51Grow, the timer reloaded 0x2D.
extern "C" void __cdecl EffectKind51_Grow2(void) { Kind51Grow(0x2D); }

// original 0x47BF40 (state 8): Kind51Wait (the same code as state 2's).
extern "C" void __cdecl EffectKind51_Wait2(void) { Kind51Wait(); }

// original 0x47BF80 (state 9): the ring, the sway stepped; when the timer's low
// two bits are 0 the width +0x2C down one; the radius +0x32 down one; the timer
// down, at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind51_RingFade(void) {
    Ring();
    SwayStep();
    unsigned char* const s = S();
    if ((s[0x5A] & 3) == 0) DecWord(s + 0x2C);
    DecWord(s + 0x32);
    DecWord(s + 0x5A);
    if (Word(s + 0x5A) == 0) SH_CALL(Effect_Release)();
}

// original 0x47BFE0 (state 10): EffectGte_LoadMapCamera; the record's point
// (+0x34, +0x38, +0x3C, copied to a local) projected to its +0x74 (the screen x,
// y and depth floats); the radius +0x32 = 0x140, the timer = 0x14, +1 up; the
// two draw moves; the ring.
extern "C" void __cdecl EffectKind51_Burst(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const s = S();
    const long point[4] = {Long(s + 0x34), Long(s + 0x38), Long(s + 0x3C), 0};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(s + 0x74));
    SetWord(S() + 0x32, 0x140);
    SetWord(S() + 0x5A, 0x14);
    NextState();
    SH_CALL(EffectKind51_DrawMoves)();
    Ring();
}

// original 0x47C050 (state 11): the ring; the radius +0x32 down 0x10; the timer
// down, at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind51_RingClose(void) {
    Ring();
    unsigned char* const s = S();
    SetWord(s + 0x32, Word(s + 0x32) - 0x10u);
    DecWord(s + 0x5A);
    if (Word(S() + 0x5A) == 0) SH_CALL(Effect_Release)();
}

// original 0x47C6E0 (0xF6 bytes): two draw-move primitives at the record's
// screen point. rect = (ftol(x - 64.0f), ftol(Gfx_BufferIndex * 0xF0 + (y -
// 64.0f)), 0x40, 0x80) (x, y the floats +0x74, +0x78; 64.0f at 0x5C41E4),
// Gpu_SetDrawMove(cursor, rect, 0x340, 0x100), committed 0x18 in slot 2; then,
// Sprite_Current read again, rect = (ftol(x), the same y, 0x40, 0x80),
// Gpu_SetDrawMove(cursor, rect, 0x340, 0x180), committed likewise. Each ftol is
// the CRT's (truncation), its low word kept.
extern "C" void __cdecl EffectKind51_DrawMoves(void) {
    unsigned char rect[8];
    const unsigned char* s = S();
    SetWord(rect, Ftol(X(s + 0x74) - 64.0L));
    SetWord(rect + 2, Ftol(XI(Gfx_BufferIndex * 0xF0u) + (X(s + 0x78) - 64.0L)));
    SetWord(rect + 4, 0x40);
    SetWord(rect + 6, 0x80);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x340, 0x100);
    SH_CALL(Gfx_CommitPrim)(2, 0x18);
    s = S();
    SetWord(rect, Ftol(X(s + 0x74)));
    SetWord(rect + 2, Ftol(XI(Gfx_BufferIndex * 0xF0u) + (X(s + 0x78) - 64.0L)));
    SetWord(rect + 4, 0x40);
    SetWord(rect + 6, 0x80);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x340, 0x180);
    SH_CALL(Gfx_CommitPrim)(2, 0x18);
}

// original 0x47C7E0: EffectKind51_Angles' words 0..63 moved up one (from the
// top down), `angle`'s low word into word 0.
extern "C" void __cdecl EffectKind51_PushAngle(unsigned angle) {
    unsigned char* const w = reinterpret_cast<unsigned char*>(EffectKind51_Angles);
    for (unsigned i = at::kAngles - 1; i >= 1; --i) SetWord(w + 2 * i, Word(w + 2 * (i - 1)));
    SetWord(w, angle);
}

// original 0x47C810: EffectGte_LoadMapCamera; the record's point projected to
// its +0x74 (as EffectKind51_Burst); the two draw moves; the sway +0x58 pushed
// into EffectKind51_Angles; the column at the half-height +0x32 and the width
// +0x2C (each read after the call before it).
extern "C" void __cdecl EffectKind51_DrawFrame(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const s = S();
    const long point[4] = {Long(s + 0x34), Long(s + 0x38), Long(s + 0x3C), 0};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(s + 0x74));
    SH_CALL(EffectKind51_DrawMoves)();
    SH_CALL(EffectKind51_PushAngle)(Word(S() + 0x58));
    const unsigned char* const t = S();
    SH_CALL(EffectKind51_DrawColumn)(Word(t + 0x32), Word(t + 0x2C));
}

// original 0x47C870 (0x359 bytes): the column, two textured Gouraud quads a row
// for rows i = 0 .. 2 h - 1 (h = `half` as s16; nothing when 2 h <= 0). Row i:
// k = h - 1 - i (s16), w = the root of h h - k k (0x5A7A90: fild, fsqrt,
// _ftol), dx = sin(EffectKind51_Angles[i]) * sway sar 12 (sway s16); as floats
// (each fild / fstp to a local) w, dx, h, i. The left quad: x from x0 - w + dx
// to that + w, y from y0 - h + i to that + 1.0f (x0, y0 the floats +0x74,
// +0x78; the sums in the original's order), shaded 0x40 / 0x40 / 0x80 and 0x60,
// u 0x40 - w's low byte and 0x40 (i - h low byte + 0x40 / + 0x41 the v's); the
// right quad from dx + x0 to that + w, u 0 and w's low byte (v's i - h - 0x40 /
// - 0x3F), shaded 0x60 and 0x80 / 0x40 / 0x40; the depth +0x7C at every vertex;
// the page Gpu_GetTPage(2, 0, 0x340, 0x100); each committed 0x54 in slot 2. The
// row counter is a byte (the original compares it zero-extended against 2 h),
// the angles 65: a row past them is the original reading past
// EffectKind51_Angles (and past row 255 it would never end) - ours aborts
// there. The kind's states keep h at most 0x20 (docs/effect_2f.md section 2).
extern "C" void __cdecl EffectKind51_DrawColumn(unsigned half, unsigned sway) {
    const U h = S16(half);
    const U n = h + h;
    if (I(n) <= 0) return;
    const LD fh = AsFloat(h);
    const U hh = h * h;
    const U width = S16(sway);
    const auto h_byte = static_cast<unsigned char>(half);
    U k = half - 1u;
    U i = 0;
    do {
        const U kk = S16(k);
        const U w = static_cast<U>(SH_AT(long (__cdecl*)(long), at::kSqrt)(static_cast<long>(hh - kk * kk)));
        if (i >= at::kAngles)
            bof3::Fatal("EffectKind51_DrawColumn: row %u of %d, past the %u words of EffectKind51_Angles - the original "
                        "reads past them (docs/effect_2f.md section 2)",
                        (unsigned)i, I(n), at::kAngles);
        const U angle = Word(reinterpret_cast<const unsigned char*>(EffectKind51_Angles) + 2 * i);
        const U dx = Scale(SH_CALL(Math_Sin)(static_cast<int>(angle)), width);
        unsigned char* prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        const LD fw = AsFloat(w);
        const LD fdx = AsFloat(dx);
        const LD fi = AsFloat(i);
        Fstp(prim + 8, (X(S() + 0x74) - fw) + fdx);
        Fstp(prim + 0xC, (X(S() + 0x78) - fh) + fi);
        Fstp(prim + 0x1C, ((X(S() + 0x74) - fw) + fdx) + fw);
        Fstp(prim + 0x20, (X(S() + 0x78) - fh) + fi);
        Fstp(prim + 0x30, (X(S() + 0x74) - fw) + fdx);
        Fstp(prim + 0x34, ((X(S() + 0x78) - fh) + fi) + 1.0L);
        Fstp(prim + 0x44, ((X(S() + 0x74) - fw) + fdx) + fw);
        const auto row = static_cast<unsigned char>(static_cast<unsigned char>(i) - h_byte);
        const auto u = static_cast<unsigned char>(0x40 - static_cast<unsigned char>(w));
        Fstp(prim + 0x48, ((X(S() + 0x78) - fh) + fi) + 1.0L);
        prim[0x14] = u;
        prim[0x3C] = u;
        prim[0x15] = static_cast<unsigned char>(row + 0x40);
        prim[0x28] = 0x40;
        prim[0x29] = static_cast<unsigned char>(row + 0x40);
        prim[0x3D] = static_cast<unsigned char>(row + 0x41);
        prim[0x50] = 0x40;
        prim[0x51] = static_cast<unsigned char>(row + 0x41);
        const unsigned char* s = S();
        FCopy(prim + 0x4C, s + 0x7C);
        FCopy(prim + 0x38, s + 0x7C);
        FCopy(prim + 0x24, s + 0x7C);
        FCopy(prim + 0x10, s + 0x7C);
        SetWord(prim + 0x2A, SH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100));
        prim[5] = 0x40;
        prim[4] = 0x40;
        prim[6] = 0x80;
        prim[0x1A] = prim[0x19] = prim[0x18] = 0x60;
        prim[0x2D] = prim[0x2C] = 0x40;
        prim[0x2E] = 0x80;
        prim[0x42] = prim[0x41] = prim[0x40] = 0x60;
        SH_CALL(Gfx_CommitPrim)(2, 0x54);
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        Fstp(prim + 8, fdx + X(S() + 0x74));
        Fstp(prim + 0xC, (X(S() + 0x78) - fh) + fi);
        Fstp(prim + 0x1C, (fdx + X(S() + 0x74)) + fw);
        Fstp(prim + 0x20, (X(S() + 0x78) - fh) + fi);
        Fstp(prim + 0x30, fdx + X(S() + 0x74));
        Fstp(prim + 0x34, ((X(S() + 0x78) - fh) + fi) + 1.0L);
        Fstp(prim + 0x44, (fdx + X(S() + 0x74)) + fw);
        const auto w_byte = static_cast<unsigned char>(w);
        const auto v0 = static_cast<unsigned char>(row - 0x40);
        const auto v1 = static_cast<unsigned char>(row - 0x3F);
        prim[0x14] = 0;
        prim[0x15] = v0;
        prim[0x28] = w_byte;
        prim[0x29] = v0;
        prim[0x3C] = 0;
        prim[0x3D] = v1;
        prim[0x50] = w_byte;
        prim[0x51] = v1;
        Fstp(prim + 0x48, ((X(S() + 0x78) - fh) + fi) + 1.0L);
        s = S();
        FCopy(prim + 0x4C, s + 0x7C);
        FCopy(prim + 0x38, s + 0x7C);
        FCopy(prim + 0x24, s + 0x7C);
        FCopy(prim + 0x10, s + 0x7C);
        SetWord(prim + 0x2A, SH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100));
        prim[6] = prim[5] = 0x60;
        prim[4] = 0x60;
        prim[0x18] = 0x80;
        prim[0x1A] = prim[0x19] = 0x40;
        prim[0x2E] = prim[0x2D] = prim[0x2C] = 0x60;
        prim[0x40] = 0x80;
        prim[0x42] = prim[0x41] = 0x40;
        SH_CALL(Gfx_CommitPrim)(2, 0x54);
        k -= 1u;
        i = (i + 1u) & 0xFFu;
    } while (I(i) < I(n));
}

// original 0x47CBD0 (0x330 bytes): the ring, sixteen pairs of textured Gouraud
// quads for a = 0xFC00, 0xFC80, .. (each & 0xFFFF; b = a + 0x80 & 0xFFFF), r =
// `radius` as s16, x0 / y0 the floats +0x74 / +0x78, the depth +0x7C at every
// vertex. The outer quad: vertices x0 - (cos(a) r sar 12) (the +0x74 address
// taken before the call), x0 (copied) and the same at b; y0 + (sin r sar 12) at
// both; u 0x3F - (cos << 5 sar 12) / 0x3F, v (sin << 5 sar 12) + 0x40; shaded
// 0x40 / 0x40 / 0x80 and 0x60. The inner quad: x0 (copied) and x0 + (cos r sar
// 12), the same y's; u 0 / (cos << 5 sar 12), v (sin << 5 sar 12) - 0x40;
// shaded 0x60 and 0x80 / 0x40 / 0x40. The page Gpu_GetTPage(2, 0, 0x340,
// 0x100); each committed 0x54 in slot 2. Every Math_* in the original's order.
extern "C" void __cdecl EffectKind51_DrawRing(unsigned radius) {
    const U r = S16(radius);
    U a = 0xFC00;
    for (unsigned n = 0; n < 16; ++n) {
        const U next = a + 0x80u;
        const U ea = a & 0xFFFFu;
        const U eb = next & 0xFFFFu;
        unsigned char* prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        const unsigned char* x0 = S() + 0x74;
        U v = Scale(SH_CALL(Math_Cos)(I(ea)), r);
        Fstp(prim + 8, X(x0) - XI(v));
        std::memcpy(prim + 0x1C, S() + 0x74, 4);
        v = Scale(SH_CALL(Math_Sin)(I(ea)), r);
        LD t = XI(v) + X(S() + 0x78);
        Fstp(prim + 0x20, t);
        Fstp(prim + 0xC, t);
        x0 = S() + 0x74;
        v = Scale(SH_CALL(Math_Cos)(I(eb)), r);
        Fstp(prim + 0x30, X(x0) - XI(v));
        std::memcpy(prim + 0x44, S() + 0x74, 4);
        v = Scale(SH_CALL(Math_Sin)(I(eb)), r);
        t = XI(v) + X(S() + 0x78);
        Fstp(prim + 0x48, t);
        Fstp(prim + 0x34, t);
        const unsigned char* s = S();
        FCopy(prim + 0x4C, s + 0x7C);
        FCopy(prim + 0x38, s + 0x7C);
        FCopy(prim + 0x24, s + 0x7C);
        FCopy(prim + 0x10, s + 0x7C);
        unsigned char c = static_cast<unsigned char>(0x3F - Shade(SH_CALL(Math_Cos)(I(ea))));
        prim[0x28] = 0x3F;
        prim[0x14] = c;
        c = static_cast<unsigned char>(Shade(SH_CALL(Math_Sin)(I(ea))) + 0x40);
        prim[0x29] = c;
        prim[0x15] = c;
        c = static_cast<unsigned char>(0x3F - Shade(SH_CALL(Math_Cos)(I(eb))));
        prim[0x50] = 0x3F;
        prim[0x3C] = c;
        c = static_cast<unsigned char>(Shade(SH_CALL(Math_Sin)(I(eb))) + 0x40);
        prim[0x51] = c;
        prim[0x3D] = c;
        prim[5] = 0x40;
        prim[4] = 0x40;
        prim[6] = 0x80;
        prim[0x1A] = prim[0x19] = prim[0x18] = 0x60;
        prim[0x2D] = prim[0x2C] = 0x40;
        prim[0x2E] = 0x80;
        prim[0x42] = prim[0x41] = prim[0x40] = 0x60;
        SetWord(prim + 0x2A, SH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100));
        SH_CALL(Gfx_CommitPrim)(2, 0x54);
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        std::memcpy(prim + 8, S() + 0x74, 4);
        v = Scale(SH_CALL(Math_Cos)(I(ea)), r);
        Fstp(prim + 0x1C, XI(v) + X(S() + 0x74));
        v = Scale(SH_CALL(Math_Sin)(I(ea)), r);
        t = XI(v) + X(S() + 0x78);
        Fstp(prim + 0x20, t);
        Fstp(prim + 0xC, t);
        std::memcpy(prim + 0x30, S() + 0x74, 4);
        v = Scale(SH_CALL(Math_Cos)(I(eb)), r);
        Fstp(prim + 0x44, XI(v) + X(S() + 0x74));
        v = Scale(SH_CALL(Math_Sin)(I(eb)), r);
        t = XI(v) + X(S() + 0x78);
        Fstp(prim + 0x48, t);
        Fstp(prim + 0x34, t);
        s = S();
        prim[0x14] = 0;
        FCopy(prim + 0x4C, s + 0x7C);
        FCopy(prim + 0x38, s + 0x7C);
        FCopy(prim + 0x24, s + 0x7C);
        FCopy(prim + 0x10, s + 0x7C);
        prim[0x28] = Shade(SH_CALL(Math_Cos)(I(ea)));
        c = static_cast<unsigned char>(Shade(SH_CALL(Math_Sin)(I(ea))) - 0x40);
        prim[0x29] = c;
        prim[0x15] = c;
        prim[0x3C] = 0;
        prim[0x50] = Shade(SH_CALL(Math_Cos)(I(eb)));
        c = static_cast<unsigned char>(Shade(SH_CALL(Math_Sin)(I(eb))) - 0x40);
        prim[0x51] = c;
        prim[0x3D] = c;
        prim[6] = prim[5] = prim[4] = 0x60;
        prim[0x18] = 0x80;
        prim[0x1A] = prim[0x19] = 0x40;
        prim[0x2E] = prim[0x2D] = prim[0x2C] = 0x60;
        prim[0x40] = 0x80;
        prim[0x42] = prim[0x41] = 0x40;
        SetWord(prim + 0x2A, SH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100));
        SH_CALL(Gfx_CommitPrim)(2, 0x54);
        a = next;
    }
}

// ===========================================================================
// Kind 0x52: Effect_KindHandlers[0x52] (0x655498), EffectKind52_States (two)
// ===========================================================================

namespace {

unsigned char* Trail() { return At(at::kTrail); }

// The timer byte +9 down; at 0, +9 = `reload` and +2 up.
void SubCountDown(unsigned char reload) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    s[9] = reload;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// The record put at (x, z) (16.16 cells) on the ground: +0x3C =
// AreaMap_Elevation(+0x34, +0x38)'s low word << 16; +9 = 0x20, +2 up and
// Sound_PlayEffect(0x202).
void TrailPlace(U x, U z) {
    SetUL(S() + 0x34, x);
    SetUL(S() + 0x38, z);
    const unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetUL(S() + 0x3C, S16(static_cast<U>(ground)) << 16);
    S()[9] = 0x20;
    NextSub();
    SH_CALL(Sound_PlayEffect)(0x202);
}

}  // namespace

// original 0x47C090: jmp [EffectKind52_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind52_Run(void) {
    Jump("EffectKind52_Run", AddressOf(EffectKind52_States), EffectKind52_States_count, S()[1]);
}

// original 0x47C0B0 (state 0): jmp [EffectKind52_SparkSubStates + +2 * 4],
// unbounded.
extern "C" void __cdecl EffectKind52_Sparks(void) {
    Jump("EffectKind52_Sparks", AddressOf(EffectKind52_SparkSubStates), EffectKind52_SparkSubStates_count, S()[2]);
}

// original 0x47C0D0 (state 0, sub-state 0): the sparks cleared, the timer +0x5A
// = 0x40, +2 up.
extern "C" void __cdecl EffectKind52_SparksClear(void) {
    SH_CALL(EffectKind52_ClearSparks)();
    SetWord(S() + 0x5A, 0x40);
    NextSub();
}

// original 0x47C0F0 (sub-state 1; catalog part 7, in no group): at timer 4
// Sound_PlayEffect(0x201); when the timer's low three bits are 0 a free spark
// set going (0x4790F0); the sparks moved and drawn; the timer down, at 0 +2 up.
extern "C" void __cdecl EffectKind52_SparksEmit(void) {
    if (Word(S() + 0x5A) == 4) SH_CALL(Sound_PlayEffect)(0x201);
    if ((S()[0x5A] & 7) == 0) {
        unsigned char* const spark = SH_CALL(EffectSpark_FindFree)();
        if (spark != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kSparkInit)(spark);
    }
    SH_CALL(EffectKind52_MoveSparks)();
    unsigned char* const s = S();
    DecWord(s + 0x5A);
    if (Word(s + 0x5A) == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x47C150 (sub-state 2): the sparks moved and drawn; once none was
// in use, a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind52_SparksDrain(void) {
    if (SH_CALL(EffectKind52_MoveSparks)() == 0) SH_CALL(Effect_Release)();
}

// original 0x47C160 (state 1): a call through EffectKind52_TrailSubStates by +2
// (unbounded), then the trail at 0x92C060 updated and drawn.
extern "C" void __cdecl EffectKind52_Trail(void) {
    Jump("EffectKind52_Trail", AddressOf(EffectKind52_TrailSubStates), EffectKind52_TrailSubStates_count, S()[2]);
    SH_CALL(EffectKind52_TrailUpdate)(Trail());
    SH_CALL(EffectKind52_TrailDraw)(Trail());
}

// original 0x47C190 (sub-state 0; catalog part 7, in no group): the trail's
// width (its last word, 0x92C49E) = 0xC0; the record put at (0x6B, 0x46) on the
// ground.
extern "C" void __cdecl EffectKind52_TrailStart(void) {
    SetWord(At(at::kTrailSize), 0xC0);
    TrailPlace(0x6B0000, 0x460000);
}

// original 0x47C200 (sub-state 1): z +0x38 up one cell; +9 down, at 0 +9 = 0x10
// and +2 up.
extern "C" void __cdecl EffectKind52_TrailRise(void) {
    unsigned char* const s = S();
    SetUL(s + 0x38, UL(s + 0x38) + 0x10000u);
    SubCountDown(0x10);
}

// original 0x47C240 (sub-state 2): +9 down; at 0 the record put at (0x64,
// 0x76) on the ground.
extern "C" void __cdecl EffectKind52_TrailTurn(void) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    TrailPlace(0x640000, 0x760000);
}

// original 0x47C2B0 (sub-state 3): z +0x38 down one cell; +9 down, at 0 +9 =
// 0x80 and +2 up.
extern "C" void __cdecl EffectKind52_TrailFall(void) {
    unsigned char* const s = S();
    SetUL(s + 0x38, UL(s + 0x38) + 0xFFFF0000u);
    SubCountDown(0x80);
}

// original 0x47C2F0 (sub-state 4): +9 down, at 0 +9 = 8 and +2 up.
extern "C" void __cdecl EffectKind52_TrailHold(void) { SubCountDown(8); }

// original 0x47C320 (sub-state 5; catalog part 7, in no group): the trail's
// width down 0x18; +9 down, at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind52_TrailFade(void) {
    unsigned char* const s = S();
    unsigned char* const size = At(at::kTrailSize);
    SetWord(size, Word(size) - 0x18u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) SH_CALL(Effect_Release)();
}

// original 0x47CF00: +0 of each of the 8 sparks (0x1C apart from
// EffectKind30_Shards) = 0.
extern "C" void __cdecl EffectKind52_ClearSparks(void) {
    for (unsigned i = 0; i < at::kSparks; ++i) EffectKind30_Shards[i * at::kSparkStride] = 0;
}

// original 0x47CF20 (catalog part 7, in no group): the first of the 8 sparks
// whose +0 is 0, or null. The spark pool is shared: E2D's 0x4785B0 and E2E's
// 0x478CC0 call this too.
extern "C" unsigned char* __cdecl EffectSpark_FindFree(void) {
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const k = EffectKind30_Shards + i * at::kSparkStride;
        if (k[0] == 0) return k;
    }
    return nullptr;
}

// original 0x47CF40 (catalog part 7, in no group): a draw mode
// (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, committed 0xC), EffectGte_LoadMapCamera;
// each spark in use called through EffectKind52_SparkStates by its +1
// (unbounded; each handed the spark) and drawn (0x4792E0). Answers 1 when a
// spark was in use, else 0.
extern "C" unsigned char __cdecl EffectKind52_MoveSparks(void) {
    DrawMode(0, 1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const k = EffectKind30_Shards + i * at::kSparkStride;
        if (k[0] == 0) continue;
        const unsigned state = k[1];
        if (state >= EffectKind52_SparkStates_count)
            bof3::Fatal("EffectKind52_MoveSparks: spark %u's state +1 is %u, past the %u entries of EffectKind52_SparkStates - "
                        "the original calls through the dword after (docs/effect_2f.md section 2)",
                        i, state, EffectKind52_SparkStates_count);
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(
            static_cast<std::uintptr_t>(UL(At(AddressOf(EffectKind52_SparkStates) + 4 * state))))(k);
        SH_AT(void (__cdecl*)(unsigned char*), at::kSparkDraw)(k);
        any = 1;
    }
    return any;
}

// original 0x47CFC0 (EffectKind52_SparkStates[0]; catalog part 7, in no group):
// the spark's shade +3 up 6; its count +2 down, at 0 the count = its reload +4
// and +1 up.
extern "C" void __cdecl EffectKind52_SparkGlow(unsigned char* spark) {
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] != 0) return;
    spark[2] = spark[4];
    spark[1] = static_cast<unsigned char>(spark[1] + 1);
}

// original 0x47CFF0 (EffectKind52_SparkStates[1] and entry 1 of the table at
// 0x654660 E2E's 0x479260 calls through; catalog part 7): the count +2 down, at
// 0 the count = 0x20 and +1 up.
extern "C" void __cdecl EffectSpark_Wait(unsigned char* spark) {
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] != 0) return;
    spark[2] = 0x20;
    spark[1] = static_cast<unsigned char>(spark[1] + 1);
}

// original 0x47D010 (EffectKind52_SparkStates[2]; catalog part 7): the rise
// speed +8 up 0x40000, the height +0x14 up by it; the shade +3 up 6; the count
// +2 down, at 0 the spark freed (+0 = 0).
extern "C" void __cdecl EffectKind52_SparkRise(unsigned char* spark) {
    const U speed = UL(spark + 8) + 0x40000u;
    SetUL(spark + 8, speed);
    SetUL(spark + 0x14, UL(spark + 0x14) + speed);
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] == 0) spark[0] = 0;
}

// original 0x47D040 (0x1D1 bytes): the trail's 32 points of 0x20 moved down one
// (point 30 to 31 .. 0 to 1, 0x20 bytes each), the record's point (+0x34,
// +0x38, +0x3C) into point 0; EffectGte_LoadMapCamera; each point projected to
// its +0x10 (x, y, depth floats) and its width +0x1E = the trail's width word
// (+0x43E, as both words of a size) scaled at its depth (EffectGte_ProjectSize's
// first answer). Then the 31 angles at +0x400: from point k to point k + 1,
// dx = ftol(x_k - x_k+1), dy likewise (low words); both 0: 0x1000 (none), else
// Math_Ratan2(dy, dx) as floats. The nones filled in order: each 0x1000 takes
// the first later angle that is not 0x1000, else the first earlier one (filled
// ones included), else 0 (the original reloads its 0x1000 at every step, so a
// fill never changes what the later ones are compared with). Point 0's angle +0x1C =
// angle 0, points 1..30 the mean (0x479970, E2E's) of angles k - 1 and k, point
// 31's = angle 30.
extern "C" void __cdecl EffectKind52_TrailUpdate(unsigned char* trail) {
    for (int k = 0x1E; k >= 0; --k) std::memcpy(trail + 0x20 * (k + 1), trail + 0x20 * k, 0x20);
    SetUL(trail, UL(S() + 0x34));
    SetUL(trail + 4, UL(S() + 0x38));
    SetUL(trail + 8, UL(S() + 0x3C));
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned k = 0; k < 0x20; ++k) {
        unsigned char* const p = trail + 0x20 * k;
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p), reinterpret_cast<float*>(p + 0x10));
        const auto width = static_cast<short>(Word(trail + 0x43E));
        short size[2] = {width, width};
        short out[2] = {0, 0};
        SH_CALL(EffectGte_ProjectSize)(reinterpret_cast<const long*>(p), size, out);
        SetWord(p + 0x1E, static_cast<unsigned short>(out[0]));
    }
    unsigned char* const angles = trail + 0x400;
    for (unsigned k = 0; k < 0x1F; ++k) {
        const unsigned char* const p = trail + 0x10 + 0x20 * k;
        const unsigned char* const q = p + 0x20;
        const U dx = Ftol(X(p) - X(q)) & 0xFFFFu;
        const U dy = Ftol(X(p + 4) - X(q + 4)) & 0xFFFFu;
        if (dx == 0 && dy == 0) {
            SetWord(angles + 2 * k, 0x1000);
            continue;
        }
        const auto fx = static_cast<float>(XI(S16(dx)));
        const auto fy = static_cast<float>(XI(S16(dy)));
        SetWord(angles + 2 * k, static_cast<U>(SH_CALL(Math_Ratan2)(fy, fx)));
    }
    for (int c = 0; c < 0x1F; ++c) {
        unsigned char* const at_c = angles + 2 * c;
        if (Word(at_c) != 0x1000) continue;
        int found = -1;
        for (int a = c + 1; a < 0x1F && found < 0; ++a)
            if (Word(angles + 2 * a) != 0x1000) found = a;
        for (int a = 0; a < c && found < 0; ++a)
            if (Word(angles + 2 * a) != 0x1000) found = a;
        SetWord(at_c, found < 0 ? 0u : Word(angles + 2 * found));
    }
    SetWord(trail + 0x1C, Word(angles));
    for (unsigned k = 0; k < 0x1E; ++k)
        SetWord(trail + 0x3C + 0x20 * k,
                static_cast<U>(SH_AT(long (__cdecl*)(unsigned, unsigned), at::kAngleMean)(Word(angles + 2 * k),
                                                                                          Word(angles + 2 * k + 2))));
    SetWord(trail + 0x3FC, Word(trail + 0x43C));
}

// original 0x47D220 (0x2D0 bytes): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1, committed 0xC); the cap at point 0 (EffectTrail_DrawCap at its angle -
// 0x400, shade 0x80); then for each pair of points k, k + 1 (screen floats at
// +0x10) whose x or y differ (fcomp: equal or unordered both = skipped), two
// Gouraud quads at the packet cursor, semi-transparent: the first from point k's
// screen x, y and x + (cos(a + 0x400) w sar 12), y + (sin ..) to point k + 1's
// likewise (a, w each point's angle +0x1C and width +0x1E, both zero-extended),
// the depths +0x18 at +0x10 +0x20 / +0x30 +0x40, shaded the running shade at
// point k's vertices and it - 4 at k + 1's (black at the offset ones);
// committed 0x44 in slot 1; the second a copy of the first (0x44 bytes from the
// cursor back) with the offset vertices at a - 0x400; committed; the running
// shade down 4. The cap at point 31 at its angle + 0x400 and the running shade.
extern "C" void __cdecl EffectKind52_TrailDraw(unsigned char* trail) {
    DrawMode(0, 1, 0x3C0, 0, 1, 1);
    unsigned char shade = 0x80;
    SH_CALL(EffectTrail_DrawCap)(trail + 0x10, Word(trail + 0x1E), (Word(trail + 0x1C) - 0x400u) & 0xFFFFu, 0x80);
    for (unsigned k = 0; k < 0x1F; ++k) {
        const unsigned char* const p = trail + 0x10 + 0x20 * k;
        const unsigned char* const q = p + 0x20;
        if (EqualOrUnordered(X(p), X(q)) && EqualOrUnordered(X(p + 4), X(q + 4))) continue;
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, p, 4);
        std::memcpy(prim + 0xC, p + 4, 4);
        U v = Sar(static_cast<U>(SH_CALL(Math_Cos)(I(Word(p + 0xC) + 0x400u))) * Word(p + 0xE), 12);
        Fstp(prim + 0x18, XI(v) + X(p));
        v = Sar(static_cast<U>(SH_CALL(Math_Sin)(I(Word(p + 0xC) + 0x400u))) * Word(p + 0xE), 12);
        Fstp(prim + 0x1C, XI(v) + X(p + 4));
        FCopy(prim + 0x20, p + 8);
        FCopy(prim + 0x10, p + 8);
        std::memcpy(prim + 0x28, q, 4);
        std::memcpy(prim + 0x2C, q + 4, 4);
        v = Sar(static_cast<U>(SH_CALL(Math_Cos)(I(Word(q + 0xC) + 0x400u))) * Word(q + 0xE), 12);
        Fstp(prim + 0x38, XI(v) + X(q));
        v = Sar(static_cast<U>(SH_CALL(Math_Sin)(I(Word(q + 0xC) + 0x400u))) * Word(q + 0xE), 12);
        const auto next = static_cast<unsigned char>(shade - 4);
        Fstp(prim + 0x3C, XI(v) + X(q + 4));
        FCopy(prim + 0x40, q + 8);
        FCopy(prim + 0x30, q + 8);
        prim[6] = prim[5] = shade;
        prim[4] = shade;
        prim[0x16] = prim[0x15] = prim[0x14] = 0;
        prim[0x26] = prim[0x25] = prim[0x24] = next;
        prim[0x36] = prim[0x35] = prim[0x34] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        unsigned char* const copy = Gfx_PacketNext;
        std::memcpy(copy, copy - 0x44, 0x44);
        v = Sar(static_cast<U>(SH_CALL(Math_Cos)(I(Word(p + 0xC) - 0x400u))) * Word(p + 0xE), 12);
        Fstp(copy + 0x18, XI(v) + X(p));
        v = Sar(static_cast<U>(SH_CALL(Math_Sin)(I(Word(p + 0xC) - 0x400u))) * Word(p + 0xE), 12);
        Fstp(copy + 0x1C, XI(v) + X(p + 4));
        v = Sar(static_cast<U>(SH_CALL(Math_Cos)(I(Word(q + 0xC) - 0x400u))) * Word(q + 0xE), 12);
        Fstp(copy + 0x38, XI(v) + X(q));
        v = Sar(static_cast<U>(SH_CALL(Math_Sin)(I(Word(q + 0xC) - 0x400u))) * Word(q + 0xE), 12);
        Fstp(copy + 0x3C, XI(v) + X(q + 4));
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        shade = static_cast<unsigned char>(shade - 4);
    }
    SH_CALL(EffectTrail_DrawCap)(trail + 0x3F0, Word(trail + 0x3FE), (Word(trail + 0x3FC) + 0x400u) & 0xFFFFu, shade);
}

// original 0x47D4F0 (0x109 bytes): eight Gouraud triangles at the packet cursor,
// semi-transparent, a half fan round `point` (screen x, y, depth floats): for a
// = angle, angle + 0x100, .. (each & 0xFFFF; b = a + 0x100), the centre (copied)
// and (x + (cos r sar 12), y + (sin r sar 12)) at a and at b (r = `size` as
// s16), the depth at all three; the centre shaded `shade` (its low byte), the
// rim black; committed 0x34 in slot 1.
extern "C" void __cdecl EffectTrail_DrawCap(const unsigned char* point, unsigned size, unsigned angle, unsigned shade) {
    const U r = S16(size);
    const auto centre = static_cast<unsigned char>(shade);
    U slot = angle;
    U a = angle & 0xFFFFu;
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, point, 4);
        std::memcpy(prim + 0xC, point + 4, 4);
        U v = Scale(SH_CALL(Math_Cos)(I(a)), r);
        Fstp(prim + 0x18, XI(v) + X(point));
        v = Scale(SH_CALL(Math_Sin)(I(a)), r);
        slot += 0x100u;
        a = slot & 0xFFFFu;
        Fstp(prim + 0x1C, XI(v) + X(point + 4));
        v = Scale(SH_CALL(Math_Cos)(I(a)), r);
        Fstp(prim + 0x28, XI(v) + X(point));
        v = Scale(SH_CALL(Math_Sin)(I(a)), r);
        Fstp(prim + 0x2C, XI(v) + X(point + 4));
        prim[6] = centre;
        prim[5] = centre;
        FCopy(prim + 0x30, point + 8);
        FCopy(prim + 0x20, point + 8);
        FCopy(prim + 0x10, point + 8);
        prim[4] = centre;
        prim[0x26] = prim[0x25] = prim[0x24] = 0;
        prim[0x16] = prim[0x15] = prim[0x14] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// ===========================================================================
// Kind 0x53: Effect_KindHandlers[0x53] (0x65549C), EffectKind53_States (two)
// ===========================================================================

// original 0x47D600 (hidden in 0x47D4F0's recorded extent): jmp
// [EffectKind53_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind53_Run(void) {
    Jump("EffectKind53_Run", AddressOf(EffectKind53_States), EffectKind53_States_count, S()[1]);
}

// original 0x47D620 (state 0): +0x54 = the dword +0x54 of the
// Sprite_ObjectsExtra record the dword +0x18 names; +9 = 0, +1 = 1.
extern "C" void __cdecl EffectKind53_Start(void) {
    unsigned char* const s = S();
    SetUL(s + 0x54, UL(Extra(UL(s + 0x18), "EffectKind53_Start") + 0x54));
    s[9] = 0;
    s[1] = 1;
}

// original 0x47D650 (0x251 bytes; PSX twin 0x801F2D20 by callers, unnamed in
// the sibling): Sprite_Current set to the Sprite_ObjectsExtra record +0x18
// names (the effect record kept); the record's point = that sprite's (+0x34,
// +0x38, +0x3C); when the sprite's +9 is not 0 the record's +9 down (the
// sprite's +0x14 below 0) or up. The texture window (0, 0xF0, 0x10, 0x10) at
// the sprite; an FT4 at the packet cursor (read once): the sprite's point plus
// MoveCmd_AttachOffset(the record's +0x1C) turned into a camera vector
// ((x sar 9) - 0x4000, (z sar 9) - 0x4000, -((height word + dy) / 2)) and
// Gte_RotTransPers'd to the quad's last vertex (its answer to the record's
// +0x60), Gte_PrimDepthFlat4_10; the quad from the top of the screen (y 0)
// down to that point's y (240.0f when it is 240 or more, at 0x5C41E8), x - 8
// to x + 8 (8.0f at 0x5C41CC); Sprite_Current put back; the CLUT (0, 0x1E3), the
// page (0, 1, 0x2C0, 0x100); u 0 / 0x10, v the record's +9 & 0x15 at the top and
// ftol(that + y) at the bottom; Gpu_SetShadeTex(0). By the depth +0x60: past
// 0x220 shaded 0x80, opaque; past 0x1E0 shaded (depth's low byte + 0x20) << 1,
// semi-transparent; either linked at the record's point (0x48); nearer, not
// linked. Then the texture window (0, 0, 0x100, 0x100) at the record.
extern "C" void __cdecl EffectKind53_Beam(void) {
    unsigned char* const self = S();
    unsigned char* const sprite = Extra(UL(self + 0x18), "EffectKind53_Beam");
    Sprite_Current = sprite;
    SetUL(self + 0x34, UL(sprite + 0x34));
    SetUL(self + 0x38, UL(S() + 0x38));
    SetUL(self + 0x3C, UL(S() + 0x3C));
    const unsigned char* s = S();
    if (s[9] != 0) {
        unsigned char c = self[9];
        c = static_cast<unsigned char>(I(UL(s + 0x14)) < 0 ? c - 1 : c + 1);
        self[9] = c;
    }
    SH_CALL(EffectKind53_TexWindow)(0, 0xF0, 0x10, 0x10);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    long offset[3] = {0, 0, 0};
    SH_CALL(MoveCmd_AttachOffset)(offset, self[0x1C]);
    s = S();
    short vertex[4] = {0, 0, 0, 0};
    vertex[0] = static_cast<short>(Sar(UL(s + 0x34) + static_cast<U>(offset[0]), 9) - 0x4000u);
    vertex[1] = static_cast<short>(Sar(UL(s + 0x38) + static_cast<U>(offset[1]), 9) - 0x4000u);
    const U height = SW(s + 0x3E) + static_cast<U>(offset[2]);
    vertex[2] = static_cast<short>(0u - Sar(height + (height >> 31), 1));
    long depth_cue = 0;
    SetUL(self + 0x60, static_cast<U>(SH_CALL(Gte_RotTransPers)(vertex, reinterpret_cast<unsigned long*>(prim + 0x38),
                                                                &depth_cue)));
    SH_CALL(Gte_PrimDepthFlat4_10)(prim);
    LD t = X(prim + 0x38) - 8.0L;
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x1C, 0);
    Fstp(prim + 8, t);
    Fstp(prim + 0x28, t);
    t = X(prim + 0x38) + 8.0L;
    Fstp(prim + 0x18, t);
    Fstp(prim + 0x38, t);
    U y;
    if (!(X(prim + 0x3C) >= 240.0L)) {
        y = UL(prim + 0x3C);
    } else {
        y = 0x43700000u;
        SetUL(prim + 0x3C, y);
    }
    SetUL(prim + 0x2C, y);
    Sprite_Current = self;
    SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0, 0x1E3));
    SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    prim[0x14] = 0;
    s = S();
    prim[0x24] = 0x10;
    prim[0x15] = static_cast<unsigned char>(s[9] & 0x15);
    prim[0x25] = static_cast<unsigned char>(s[9] & 0x15);
    prim[0x34] = 0;
    prim[0x35] = static_cast<unsigned char>(Ftol(XI(s[9] & 0x15u) + X(prim + 0x2C)));
    prim[0x44] = 0x10;
    prim[0x45] = static_cast<unsigned char>(Ftol(XI(S()[9] & 0x15u) + X(prim + 0x2C)));
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    s = S();
    const std::int32_t depth = static_cast<std::int32_t>(UL(s + 0x60));
    if (depth > 0x1E0) {
        unsigned semi = 0;
        unsigned char c = 0x80;
        if (depth <= 0x220) {
            c = static_cast<unsigned char>(static_cast<unsigned char>(s[0x60] + 0x20) << 1);
            semi = 1;
        }
        prim[4] = prim[5] = prim[6] = c;
        SH_CALL(Gpu_SetSemiTrans)(prim, semi);
        const unsigned char* const t2 = S();
        SH_CALL(MapView_LinkPrimAt)(UL(t2 + 0x34), UL(t2 + 0x38), 0, 0x48);
    }
    SH_CALL(EffectKind53_TexWindow)(0, 0, 0x100, 0x100);
}

// original 0x47D8B0 (catalog part 7, in no group): an 8-byte rect (x, y, w, h,
// each the argument's low word) taken at the packet cursor (the cursor up 8),
// then 0x5A7840(cursor, rect): a primitive of code 0xF0 holding the rect's
// address (a texture window, as Window_DrawFrame 0x595C50 makes one), linked at
// Sprite_Current's point (+0x34, +0x38), 0xC bytes.
extern "C" void __cdecl EffectKind53_TexWindow(unsigned x, unsigned y, unsigned w, unsigned h) {
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, x);
    SetWord(rect + 2, y);
    SetWord(rect + 4, w);
    SetWord(rect + 6, h);
    SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), at::kPrimRect)(Gfx_PacketNext, rect);
    const unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 0, 0xC);
}

// ===========================================================================
// Kind 0x56: Effect_KindHandlers[0x56] (0x6554A8), EffectKind56_States (four)
// ===========================================================================

// original 0x47D910 (hidden in 0x47D8B0's recorded extent): jmp
// [EffectKind56_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind56_Run(void) {
    Jump("EffectKind56_Run", AddressOf(EffectKind56_States), EffectKind56_States_count, S()[1]);
}

// original 0x47D930 (state 0): the record placed; +9 = 0, +1 = 1.
extern "C" void __cdecl EffectKind56_Start(void) {
    SH_CALL(EffectKind56_Place)();
    S()[9] = 0;
    S()[1] = 1;
}

// original 0x47D950 (state 1): once the counter byte 0x90384B is 0xA, +1 = 2.
extern "C" void __cdecl EffectKind56_Wait(void) {
    if (At(at::kCounter3)[0] == 0xA) S()[1] = 2;
}

// original 0x47D970 (state 2): +1 = 3.
extern "C" void __cdecl EffectKind56_Arm(void) { S()[1] = 3; }

// original 0x47D980 (state 3): a draw mode (Gpu_GetTPage(1, 1, 0x140, 0), dtd
// 1, committed 0xC in slot 3); the record placed; its red +0x5D = 0; from h =
// its height +0x3C - ((Frame_Counter & 0xF) << 20), while the ground +0x14 is
// below h (signed): a cross at (x, z, h), semi-transparent, the red up 0x2A (at
// most 0x80), h down one cell (2^24); then the red 0xFF and an opaque cross on
// the ground; once the counter byte 0x90384B is 0xE, +1 = 1. The step repeats
// h every 256 steps, so a walk that has not ended by then never ends (the
// ground within 2^24 of -2^31): ours aborts there.
extern "C" void __cdecl EffectKind56_Markers(void) {
    DrawMode(1, 1, 0x140, 0, 1, 3);
    SH_CALL(EffectKind56_Place)();
    const U phase = Frame_Counter & 0xFu;
    S()[0x5D] = 0;
    unsigned char* s = S();
    U h = UL(s + 0x3C) - (phase << 20);
    for (unsigned steps = 0; I(UL(s + 0x14)) < I(h); ++steps) {
        if (steps == 0x100)
            bof3::Fatal("EffectKind56_Markers: the ground 0x%X is below every height the walk from 0x%X reaches - the "
                        "original loops for ever (docs/effect_2f.md section 2)",
                        (unsigned)UL(s + 0x14), (unsigned)h);
        SH_CALL(EffectKind56_DrawCross)(Long(s + 0x34), Long(s + 0x38), static_cast<long>(h), 1);
        s = S();
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0x2A);
        if (s[0x5D] > 0x80) s[0x5D] = 0x80;
        h -= 0x1000000u;
    }
    s[0x5D] = 0xFF;
    s = S();
    SH_CALL(EffectKind56_DrawCross)(Long(s + 0x34), Long(s + 0x38), Long(s + 0x14), 0);
    if (At(at::kCounter3)[0] == 0xE) S()[1] = 1;
}

// original 0x47DA60: the record's point = Sprite_ObjectsExtra[0]'s (+0x34,
// +0x38, +0x3C); MapView_HeightScale = 1; the ground +0x14 =
// MapView_GroundAt(x, z)'s low word << 16. The callers push an argument it
// never reads.
extern "C" void __cdecl EffectKind56_Place(void) {
    const unsigned char* const extra = At(at::kExtra0Point);
    SetUL(S() + 0x34, UL(extra));
    SetUL(S() + 0x38, UL(extra + 4));
    SetUL(S() + 0x3C, UL(extra + 8));
    const unsigned char* const s = S();
    MapView_HeightScale = 1;
    const long ground = SH_CALL(MapView_GroundAt)(Long(s + 0x34), Long(s + 0x38));
    SetUL(S() + 0x14, S16(static_cast<U>(ground)) << 16);
}

// original 0x47DAC0 (0x111 bytes): two flat lines at the packet cursor, a cross
// at the world point (x, z, height): from (x + 0x4000, z) to (x - 0x4000, z),
// each end EffectGte_ProjectPoint'd into the line (+8, +0x14), coloured (the
// record's +0x5D, 0, 0), Gpu_SetSemiTrans(`semi`'s low byte), committed 0x20 in
// slot 3; then from (x, z + 0x4000) to (x, z - 0x4000), coloured likewise,
// committed, and only then Gpu_SetSemiTrans(`semi`) on it.
extern "C" void __cdecl EffectKind56_DrawCross(long x, long z, long height, unsigned semi) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const U abe = semi & 0xFFu;
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    long point[3] = {static_cast<long>(ux + 0x4000u), z, height};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 8));
    point[0] = static_cast<long>(ux - 0x4000u);
    point[1] = z;
    point[2] = height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x14));
    prim[4] = S()[0x5D];
    prim[5] = 0;
    prim[6] = 0;
    SH_CALL(Gpu_SetSemiTrans)(prim, abe);
    SH_CALL(Gfx_CommitPrim)(3, 0x20);
    prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    point[0] = x;
    point[1] = static_cast<long>(uz + 0x4000u);
    point[2] = height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 8));
    point[0] = x;
    point[1] = static_cast<long>(uz - 0x4000u);
    point[2] = height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x14));
    const unsigned char red = S()[0x5D];
    prim[5] = 0;
    prim[4] = red;
    prim[6] = 0;
    SH_CALL(Gfx_CommitPrim)(3, 0x20);
    SH_CALL(Gpu_SetSemiTrans)(prim, abe);
}

void Effect2F_Inject() {
    if (bof3::WantsShadow("effect_2f")) effect_2f::SelfTest();
    BOF3_INJECT(EffectKind4E_DrawDisc);
    BOF3_INJECT(EffectKind4F_Run);
    BOF3_INJECT(EffectKind4F_Start);
    BOF3_INJECT(EffectKind50_Run);
    BOF3_INJECT(EffectKind50_Start);
    BOF3_INJECT(EffectKind50_Emit);
    BOF3_INJECT(EffectKind50_Drain);
    BOF3_INJECT(EffectKind50_ClearSpecks);
    BOF3_INJECT(EffectKind50_MoveSpecks);
    BOF3_INJECT(EffectKind50_SpeckQuad);
    BOF3_INJECT(EffectKind50_SpawnSpeck);
    BOF3_INJECT(EffectKind50_FreeSpeck);
    BOF3_INJECT(EffectKind51_Run);
    BOF3_INJECT(EffectKind51_Start);
    BOF3_INJECT(EffectKind51_Grow);
    BOF3_INJECT(EffectKind51_Wait);
    BOF3_INJECT(EffectKind51_Shrink);
    BOF3_INJECT(EffectKind51_RingIn);
    BOF3_INJECT(EffectKind51_RingOut);
    BOF3_INJECT(EffectKind51_Start2);
    BOF3_INJECT(EffectKind51_Grow2);
    BOF3_INJECT(EffectKind51_Wait2);
    BOF3_INJECT(EffectKind51_RingFade);
    BOF3_INJECT(EffectKind51_Burst);
    BOF3_INJECT(EffectKind51_RingClose);
    BOF3_INJECT(EffectKind51_DrawMoves);
    BOF3_INJECT(EffectKind51_PushAngle);
    BOF3_INJECT(EffectKind51_DrawFrame);
    BOF3_INJECT(EffectKind51_DrawColumn);
    BOF3_INJECT(EffectKind51_DrawRing);
    BOF3_INJECT(EffectKind52_Run);
    BOF3_INJECT(EffectKind52_Sparks);
    BOF3_INJECT(EffectKind52_SparksClear);
    BOF3_INJECT(EffectKind52_SparksEmit);
    BOF3_INJECT(EffectKind52_SparksDrain);
    BOF3_INJECT(EffectKind52_Trail);
    BOF3_INJECT(EffectKind52_TrailStart);
    BOF3_INJECT(EffectKind52_TrailRise);
    BOF3_INJECT(EffectKind52_TrailTurn);
    BOF3_INJECT(EffectKind52_TrailFall);
    BOF3_INJECT(EffectKind52_TrailHold);
    BOF3_INJECT(EffectKind52_TrailFade);
    BOF3_INJECT(EffectKind52_ClearSparks);
    BOF3_INJECT(EffectSpark_FindFree);
    BOF3_INJECT(EffectKind52_MoveSparks);
    BOF3_INJECT(EffectKind52_SparkGlow);
    BOF3_INJECT(EffectSpark_Wait);
    BOF3_INJECT(EffectKind52_SparkRise);
    BOF3_INJECT(EffectKind52_TrailUpdate);
    BOF3_INJECT(EffectKind52_TrailDraw);
    BOF3_INJECT(EffectTrail_DrawCap);
    BOF3_INJECT(EffectKind53_Run);
    BOF3_INJECT(EffectKind53_Start);
    BOF3_INJECT(EffectKind53_Beam);
    BOF3_INJECT(EffectKind53_TexWindow);
    BOF3_INJECT(EffectKind56_Run);
    BOF3_INJECT(EffectKind56_Start);
    BOF3_INJECT(EffectKind56_Wait);
    BOF3_INJECT(EffectKind56_Arm);
    BOF3_INJECT(EffectKind56_Markers);
    BOF3_INJECT(EffectKind56_Place);
    BOF3_INJECT(EffectKind56_DrawCross);
}
