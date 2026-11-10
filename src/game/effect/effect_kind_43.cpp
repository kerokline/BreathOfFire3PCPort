// Moved here by tools/move_funcs.py from effect_2c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_43.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2c.h"
#include "game/effect_2c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2c::at;

}  // namespace

// DIVERGENCE DIV-0186 (D217, issue #163): kind 0x6B's particles. The original
// writes one 0x14-byte particle per opaque pixel of the 80 x 72 read-back at
// 0x92EC80 with no bound; the room before Gfx_CurrentEnv (0x937F84) holds
// 1,881, and the 1,882nd overwrites Sprite_Current - the crash of issue #163,
// measured 2026-10-29 on the owner's gaistFullRoute: the Gaist's collapsed
// body in record 2, 2,201 pixels. Ours: once g_ash_room is armed (after the
// self-test, PatchBytes "AshRoom") the particles go to a room of ours that
// holds the rectangle's every pixel (5,760), so the whole sprite crumbles;
// the count, the order, the fields and the draw are the original's. Nothing
// but these three states names the particles (a raw scan of the image: 0x92EC80
// only at 0x475F76, 0x4760E7 and, as +1, 0x476145), and the frame 0x6544A8
// that sizes them is read and never written. BOF3X_ORIGINAL=AshRoom (or
// BOF3X_OFF) leaves them at 0x92EC80, the overrun with them.
namespace effect_2c { unsigned char g_ash_room = 0; }

namespace {

using U = std::uint32_t;

using LD = long double;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULEffect2c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect2c(U a) { return ULEffect2c(At(a)); }

void SetULEffect2c(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetULEffect2c(U a, U v) { SetULEffect2c(At(a), v); }

std::int32_t SWEffect2c(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t SWEffect2c(U a) { return SWEffect2c(At(a)); }

unsigned char& B(U a) { return At(a)[0]; }

U AddressOfEffect2c(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSarEffect2c(std::int32_t a, std::int32_t b, unsigned n) { return Sar(static_cast<U>(a) * static_cast<U>(b), n); }

// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide toward zero.
std::int32_t DivPow2(U v, unsigned n) { return static_cast<std::int32_t>(v) / (1 << n); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetULEffect2c(to + i, ULEffect2c(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE (whose NaN rule
// differs); every operation in long double on the x87 at the game's control
// word, `fstp dword` on the way out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}

LD FAt(U a) { return F(At(a)); }

LD I(std::int32_t v) { return static_cast<LD>(v); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// The CRT's _ftol 0x5B9550: truncation to 64 bits, the low dword; NaN and
// values out of range give the integer indefinite 0x8000000000000000, whose
// low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775808.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + k]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, const void* table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2c.md section 6)",
                    who, state, entries, (unsigned)AddressOfEffect2c(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULEffect2c(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// Effect_FindFree's record (answer & 0xFF, as the original widens it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_2c.md section 6)",
                    who, index);
    return rv::Bytes(&Effect_Objects[index]);
}

// Kind 0x6B's column heights: 80 s16 at 0x676114 (the 0xA0 bytes its scatter
// clears); a Fatal past them, where the original reads and writes what follows.
U HeightAt(const char* who, std::int32_t column) {
    if (column < 0 || column >= static_cast<std::int32_t>(at::kHeightCount))
        bof3::Fatal("%s: column %d, past the 80 heights at 0x676114 - the original reads what lies beside them "
                    "(docs/effect_2c.md section 6)",
                    who, (int)column);
    return ::bof3::Img(EffectKind6B_Heights_addr) + 2u * static_cast<U>(column);
}

alignas(4) unsigned char g_particles[at::kPixelRoom * at::kPixelStride];

U Particles() { return effect_2c::g_ash_room ? AddressOfEffect2c(g_particles) : ::bof3::Img(at::kPixels.v); }

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

}  // namespace

// original 0x475CF0: jmp [EffectKind43_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind43_Run(void) {
    Dispatch("EffectKind43_Run", EffectKind43_States, EffectKind43_States_count, S()[1]);
}

// original 0x475D10 (state 0): EffectKind43_Setup; the word +0x32 = 0x78, the
// phase +6 = 0; +1 up; Sound_PlayEffect(0x201).
extern "C" void __cdecl EffectKind43_Start(void) {
    SH_CALL(EffectKind43_Setup)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, 0x78);
    S()[6] = 0;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x201);
}

// original 0x475D40 (state 1): four sparks added (EffectKind43_AddSpark), all
// run (EffectKind43_SparksRun); +0x32 down, at 0 the phase +6 = 1, +0x32 =
// 0x3C and +1 up.
extern "C" void __cdecl EffectKind43_Gather(void) {
    for (unsigned k = 0; k < 4; ++k) SH_CALL(EffectKind43_AddSpark)();
    SH_CALL(EffectKind43_SparksRun)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, rv::As<records::EffectWork>(S())->work_32 - 1u);
    unsigned char* s = S();
    if (rv::As<records::EffectWork>(s)->work_32 != 0) return;
    s[6] = 1;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, 0x3C);
    NextState();
}

// original 0x475D90 (state 2): the sparks run; at the counter byte 0x903848
// 0x11: Sound_PlayEffect(0x203), (0x202), the phase +6 = 2, +1 up.
extern "C" void __cdecl EffectKind43_Wait(void) {
    SH_CALL(EffectKind43_SparksRun)();
    if (B(::bof3::Img(at::kCounter.v)) != 0x11) return;
    SH_CALL(Sound_PlayEffect)(0x203);
    SH_CALL(Sound_PlayEffect)(0x202);
    S()[6] = 2;
    NextState();
}

// original 0x475DD0 (state 3): the sparks run; none live - Effect_Release (a
// tail jmp).
extern "C" void __cdecl EffectKind43_Fade(void) {
    if (SH_CALL(EffectKind43_SparksRun)() != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x475DE0: jmp [EffectKind6B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind6B_Run(void) {
    Dispatch("EffectKind6B_Run", EffectKind6B_States, EffectKind6B_States_count, S()[1]);
}

// original 0x475E00 (state 0): a free record (Effect_FindFree; none: +1 up and
// nothing else); VRAM (0x340, 0x100, 0x80, 0x100) cleared (Gfx_ClearRect);
// Sprite_Objects record 2's first 0x80 bytes copied into the record, in use,
// its point again, its screen position (+0x2E, +0x30) EffectKind6B_Frame's x
// and y, +0x24 | 0x88; with Sprite_Current the record, Sprite_UpdateScreen and
// Effect_Release (the borrowed record freed at once); Sprite_Current back, +9
// = 2 frames, +1 up.
extern "C" void __cdecl EffectKind6B_Capture(void) {
    const unsigned char found = SH_CALL(Effect_FindFree)();
    if (found == 0xFF) {
        NextState();
        return;
    }
    unsigned char* const e = NewRecord("EffectKind6B_Capture", found);
    auto* const eo = rv::As<records::EffectObject>(e);
    SH_CALL(Gfx_ClearRect)(0x340, 0x100, 0x80, 0x100);
    for (unsigned i = 0; i < at::kEffectStride; i += 4) SetULEffect2c(e + i, ULEffect2c(::bof3::Img(at::kSprite2.v) + i));
    e[0] = 1;
    eo->x = static_cast<std::uint32_t>(Sprite_Objects[2].x);
    eo->z = static_cast<std::uint32_t>(Sprite_Objects[2].z);
    eo->height = static_cast<std::uint32_t>(Sprite_Objects[2].height);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(e)->work_2e, static_cast<std::uint16_t>(EffectKind6B_Frame[0]));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(e)->work_30, static_cast<std::uint16_t>(EffectKind6B_Frame[1]));
    e[0x24] = static_cast<unsigned char>(e[0x24] | 0x88);
    unsigned char* const self = S();
    Sprite_Current = e;
    SH_CALL(Sprite_UpdateScreen)();
    SH_CALL(Effect_Release)();
    Sprite_Current = self;
    self[9] = 2;
    NextState();
}

// original 0x475EC0 (state 1): +9 down; at 0 the rectangle (0x340, 0x100, w,
// h) of EffectKind6B_Frame read back into 0x92BF80 (0x59E930, Gfx_VramShadow's
// rows) and +1 up.
extern "C" void __cdecl EffectKind6B_Store(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    alignas(4) short rect[4] = {0x340, 0x100, EffectKind6B_Frame[2], EffectKind6B_Frame[3]};
    SH_AT(void (__cdecl*)(const short*, void*), at::kStoreImage.v)(rect, At(::bof3::Img(EffectKind30_Shards_addr)));
    NextState();
}

// original 0x475F20 (state 2): the 80 column heights cleared;
// EffectGte_LoadMapCamera; Sprite_Objects record 2's point projected (o); the
// count 0x676110 = 0; for each row below h and column below w (s16, the row
// and column bytes) a pixel of the read-back rectangle that is not 0 becomes a
// particle at 0x92EC80 + 0x14 n: its colour +2, its screen point ((o.x - x +
// column) * 16, (o.y - y + row) * 16) at +4 / +8 (floats), its depth +0xC;
// below its column's height (s16) that height becomes its y through _ftol;
// its count +1 (Rand & 7) - row + h's low byte, its speed +0x10 Rand & 0x1F,
// its column +0x12; +1 up. A w or h above 0xFF never ends in the original (the
// row and column are bytes): ours aborts, as past the 80 heights.
extern "C" void __cdecl EffectKind6B_Scatter(void) {
    for (unsigned i = 0; i < 2 * at::kHeightCount; i += 4) SetULEffect2c(::bof3::Img(at::kHeights.v) + i, 0);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char q[12];
    SetULEffect2c(q, static_cast<std::uint32_t>(Sprite_Objects[2].x));
    SetULEffect2c(q + 4, static_cast<std::uint32_t>(Sprite_Objects[2].z));
    SetULEffect2c(q + 8, static_cast<std::uint32_t>(Sprite_Objects[2].height));
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o));
    U pixel = ::bof3::Img(EffectKind30_Shards_addr);
    U particle = Particles();   // DIV-0186
    EffectKind6B_PixelCount = 0;
    unsigned char row = 0;
    if (EffectKind6B_Frame[3] <= 0) {
        NextState();
        return;
    }
    if (EffectKind6B_Frame[3] > 0xFF || EffectKind6B_Frame[2] > 0xFF)
        bof3::Fatal("EffectKind6B_Scatter: the rectangle is %d x %d - the original's row and column bytes never reach "
                    "past 0xFF, it never ends (docs/effect_2c.md section 6)",
                    (int)EffectKind6B_Frame[2], (int)EffectKind6B_Frame[3]);
    if (effect_2c::g_ash_room && EffectKind6B_Frame[2] * EffectKind6B_Frame[3] > static_cast<int>(at::kPixelRoom))
        bof3::Fatal("EffectKind6B_Scatter: the rectangle is %d x %d, past DIV-0186's room of %u particles (the frame "
                    "0x6544A8 is 80 x 72 in Capcom's data and nothing writes it)",
                    (int)EffectKind6B_Frame[2], (int)EffectKind6B_Frame[3], at::kPixelRoom);
    U width = ULEffect2c(::bof3::Img(at::kFrameW.v));
    do {
        unsigned char column = 0;
        if (static_cast<std::int16_t>(width) > 0) {
            do {
                const U colour = Word(At(pixel));
                if (colour != 0) {
                    unsigned char* const p = At(particle);
                    const std::int32_t x0 = EffectKind6B_Frame[0];
                    SetWord(p + 2, colour);
                    const LD x = (F(o) - I(x0) + I(column)) * FAt(at::kSixteen);
                    const std::int32_t y0 = EffectKind6B_Frame[1];
                    const U height = HeightAt("EffectKind6B_Scatter", column);
                    const std::int32_t lowest = SWEffect2c(height);
                    SetULEffect2c(p + 0xC, ULEffect2c(o + 8));
                    StF(p + 4, x);
                    const LD y = (F(o + 4) - I(y0) + I(row)) * FAt(at::kSixteen);
                    StF(p + 8, y);
                    if (y > I(lowest)) SetWord(At(height), Ftol(F(p + 8)));
                    U r = static_cast<U>(SH_CALL(Rand)());
                    p[1] = static_cast<unsigned char>((r & 7u) - row + B(::bof3::Img(at::kFrameH.v)));
                    r = static_cast<U>(SH_CALL(Rand)());
                    SetWord(p + 0x10, r & 0x1Fu);
                    SetWord(p + 0x12, column);
                    width = ULEffect2c(::bof3::Img(at::kFrameW.v));
                    particle += at::kPixelStride;
                    EffectKind6B_PixelCount = EffectKind6B_PixelCount + 1u;
                }
                pixel += 2;
                ++column;
            } while (static_cast<std::int16_t>(column) < static_cast<std::int16_t>(width));
        }
        ++row;
    } while (static_cast<std::int16_t>(row) < SWEffect2c(::bof3::Img(at::kFrameH.v)));
    NextState();
}

// original 0x4760E0 (state 3): every particle made (the count 0x676110, a word)
// flagged 3 - live, waiting on its count; Sound_PlayEffect(0x200); +1 up.
extern "C" void __cdecl EffectKind6B_Arm(void) {
    U n = EffectKind6B_PixelCount;
    for (U p = Particles(); n != 0; --n, p += at::kPixelStride) At(p)[0] = 3;
    SH_CALL(Sound_PlayEffect)(0x200);
    NextState();
}

// original 0x476120 (state 4): Sprite_Objects record 2's +0 | 0x40; each live
// particle (bit 0 of +0) by its flags' high nibble - 0: its count +1 down, at 0
// the count 0x10 and the flags + 0x10; 0x10: its speed +0x10 up 2, its y +8 up
// by it (a float), past its column's height out of use - then drawn either way
// as a TILE_1 at (x, y) / 16, its depth, the colour the 15-bit pixel's
// channels (<< 3, >> 2, >> 7, each & 0xF8), committed to slot 2 (0x14); none
// live (or none made) - Effect_Release.
extern "C" void __cdecl EffectKind6B_Fall(void) {
    Sprite_Objects[2].flags = static_cast<unsigned char>(Sprite_Objects[2].flags | 0x40);
    unsigned char none = 1;
    if (EffectKind6B_PixelCount != 0) {
        U i = 0;
        U particle = Particles();
        do {
            unsigned char* const p = At(particle);
            const unsigned char flags = p[0];
            if (flags & 1) {
                const unsigned phase = flags & 0xF0u;
                if (phase == 0) {
                    p[1] = static_cast<unsigned char>(p[1] - 1);
                    if (p[1] == 0) {
                        const unsigned char f = p[0];
                        p[1] = 0x10;
                        p[0] = static_cast<unsigned char>(f + 0x10);
                    }
                } else if (phase == 0x10) {
                    SetWord(p + 0x10, Word(p + 0x10) + 2u);
                    const std::int32_t speed = SWEffect2c(p + 0x10);
                    const U height = HeightAt("EffectKind6B_Fall", SWEffect2c(p + 0x12));
                    const std::int32_t lowest = SWEffect2c(height);
                    const LD y = I(speed) + F(p + 8);
                    StF(p + 8, y);
                    if (y > I(lowest)) p[0] = 0;
                }
                unsigned char* const prim = Gfx_PacketNext;
                SH_CALL(Gpu_SetTile1)(prim);
                StF(prim + 8, F(p + 4) * FAt(at::kSixteenth));
                StF(prim + 0xC, F(p + 8) * FAt(at::kSixteenth));
                SetULEffect2c(prim + 0x10, ULEffect2c(p + 0xC));
                prim[4] = static_cast<unsigned char>(p[2] << 3);
                prim[5] = static_cast<unsigned char>((Word(p + 2) >> 2) & 0xF8u);
                prim[6] = static_cast<unsigned char>((Word(p + 2) >> 7) & 0xF8u);
                SH_CALL(Gfx_CommitPrim)(2, 0x14);
                none = 0;
            }
            particle += at::kPixelStride;
            ++i;
        } while ((i & 0xFFFFu) < EffectKind6B_PixelCount);
    }
    if (none != 0) SH_CALL(Effect_Release)();
}

// original 0x476230 (0x51 bytes): the record's point (0x250000, 0x748000) on
// the ground (AreaMap_Elevation's s16 << 16 into +0x3C); the 256 spark records
// of 0x1C at 0x92BF80 out of use.
extern "C" void __cdecl EffectKind43_Setup(void) {
    rv::Current<records::EffectObject>()->x = 0x250000;
    rv::Current<records::EffectObject>()->z = 0x748000;
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long ground = SH_CALL(AreaMap_Elevation)(so->x, so->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
    for (unsigned i = 0; i < at::kSparkCount43; ++i) At(::bof3::Img(EffectKind30_Shards_addr) + i * at::kSparkStride43)[0] = 0;
}

// original 0x476290 (0x32 bytes): the first spark record not in use (none:
// nothing): in use, its side +3 the byte 0x6761B4 (then flipped), its phase +2
// = 0.
extern "C" void __cdecl EffectKind43_AddSpark(void) {
    U p = ::bof3::Img(EffectKind30_Shards_addr);
    for (unsigned n = 0; At(p)[0] != 0;) {
        p += at::kSparkStride43;
        ++n;
        if ((n & 0xFFFFu) >= at::kSparkCount43) return;
    }
    const unsigned char side = EffectKind43_SparkSide;
    At(p)[0] = 1;
    At(p)[3] = side;
    At(p)[2] = 0;
    EffectKind43_SparkSide = static_cast<unsigned char>(side ^ 1);
}

// original 0x4762D0 (0x290 bytes with its five-case jump table at 0x47654C):
// EffectGte_LoadMapCamera; each live spark record (+1 a count, +2 its phase,
// +3 its side, +4 a dword, +8 an angle word, +0xC x, +0x10 z, +0x14 height) by
// its phase:
//   0  at the record's point (z 0x28000 behind, or 0x28000 ahead on side 1),
//      0x180 of height above it; +4 = ((Rand & 0x1FF) - 0x180) << 16 / 32; the
//      count 0x20; phase 1;
//   1  x - 0x2C00, z + 0xC00 (side 1: - 0xC00), height + +4; at the count's
//      end the angle (Rand & 0x1F) + 0xFC00 (side 1: + 0x400), the count 0x20
//      and the phase + the record's +6 + 1;
//   2  on a circle round the record's point ((cos - 0x5800) << 4, sin << 4) at
//      the angle, which turns 0xFF80 (side 1: 0x80) mod 0x1000; at the count's
//      end out of use;
//   3  the same without the count; the record's +6 at 2: +4 = (Rand & 0x7F) +
//      0x80, the count 0x10, phase 4;
//   4  flying out: x + cos * +4 >> 4, z + sin * +4 >> 4; at the count's end
//      out of use;
//   above 4 nothing;
// then drawn (EffectKind43_SparkDraw). al 1 when any record was live.
extern "C" unsigned char __cdecl EffectKind43_SparksRun(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    unsigned char* r = At(::bof3::Img(EffectKind30_Shards_addr));
    for (unsigned n = at::kSparkCount43; n != 0; --n, r += at::kSparkStride43) {
        if (r[0] == 0) continue;
        switch (r[2]) {
        case 0: {
            const unsigned char* const s = S();
            const bool side = r[3] != 0;
            r[1] = 0x20;
            SetULEffect2c(r + 0xC, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x));
            SetULEffect2c(r + 0x10, (side ? 0x50000u : 0u) + 0xFFFD8000u + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
            SetULEffect2c(r + 0x14, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 0x1800000u);
            const U v = ((static_cast<U>(SH_CALL(Rand)()) & 0x1FFu) - 0x180u) << 16;
            SetULEffect2c(r + 4, static_cast<U>(DivPow2(v, 5)));
            r[2] = static_cast<unsigned char>(r[2] + 1);
            break;
        }
        case 1: {
            SetULEffect2c(r + 0xC, ULEffect2c(r + 0xC) + 0xFFFFD400u);
            const unsigned char count = r[1];
            SetULEffect2c(r + 0x10, ULEffect2c(r + 0x10) + (r[3] != 0 ? 0xFFFFF400u : 0xC00u));
            SetULEffect2c(r + 0x14, ULEffect2c(r + 0x14) + ULEffect2c(r + 4));
            r[1] = static_cast<unsigned char>(count - 1);
            if (r[1] != 0) break;
            U v = static_cast<U>(SH_CALL(Rand)()) & 0x1Fu;
            v += r[3] != 0 ? 0x400u : 0xFC00u;
            const unsigned char* const s = S();
            SetWord(r + 8, v);
            r[1] = 0x20;
            r[2] = static_cast<unsigned char>(r[2] + s[6] + 1);
            break;
        }
        case 2:
        case 3: {
            const int c = SH_CALL(Math_Cos)(static_cast<int>(Word(r + 8)));
            SetULEffect2c(r + 0xC, ((static_cast<U>(c) - 0x5800u) << 4) + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
            const int sn = SH_CALL(Math_Sin)(static_cast<int>(Word(r + 8)));
            const unsigned char* const s = S();
            SetULEffect2c(r + 0x10, (static_cast<U>(sn) << 4) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
            const unsigned char count = r[1];
            SetWord(r + 8, ((r[3] != 0 ? 0x80u : 0xFF80u) + Word(r + 8)) & 0xFFFu);
            if (r[2] == 2) {
                r[1] = static_cast<unsigned char>(count - 1);
                if (r[1] == 0) r[0] = 0;
                break;
            }
            if (s[6] != 2) break;
            const U v = static_cast<U>(SH_CALL(Rand)()) & 0x7Fu;
            r[1] = 0x10;
            SetULEffect2c(r + 4, v + 0x80u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            break;
        }
        case 4: {
            const int c = SH_CALL(Math_Cos)(static_cast<int>(Word(r + 8)));
            SetULEffect2c(r + 0xC, ULEffect2c(r + 0xC) + MulSarEffect2c(c, static_cast<std::int32_t>(ULEffect2c(r + 4)), 4));
            const int sn = SH_CALL(Math_Sin)(static_cast<int>(Word(r + 8)));
            SetULEffect2c(r + 0x10, ULEffect2c(r + 0x10) + MulSarEffect2c(sn, static_cast<std::int32_t>(ULEffect2c(r + 4)), 4));
            r[1] = static_cast<unsigned char>(r[1] - 1);
            if (r[1] == 0) r[0] = 0;
            break;
        }
        default: break;
        }
        SH_CALL(EffectKind43_SparkDraw)(r);
        any = 1;
    }
    return any;
}

// original 0x476560 (0x114 bytes; in the span of 0x4762D0, in no list of the
// cut): a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 0) linked at the
// spark's point (MapView_LinkPrimAt(x, z, 0, 0xC)); its point projected; a
// semi-transparent TILE_1 there, shaded (0x80, 0x80, (Rand & 1) << 7), linked
// (0x14); a semi-transparent TILE of 3 x 3 one pixel up and left, shaded
// (0x20, 0x20, (Rand & 1) << 5), linked (0x1C).
extern "C" void __cdecl EffectKind43_SparkDraw(const unsigned char* spark) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tp & 0xFFFFu, 0);
    SH_CALL(MapView_LinkPrimAt)(ULEffect2c(spark + 0xC), ULEffect2c(spark + 0x10), 0, 0xC);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0xC), Out(o));
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    Copy12(prim + 8, o);
    prim[5] = prim[4] = 0x80;
    prim[6] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) << 7);
    SH_CALL(MapView_LinkPrimAt)(ULEffect2c(spark + 0xC), ULEffect2c(spark + 0x10), 0, 0x14);
    prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    StF(prim + 8, F(o) - FAt(at::kOne));
    StF(prim + 0xC, F(o + 4) - FAt(at::kOne));
    SetULEffect2c(prim + 0x10, ULEffect2c(o + 8));
    SetULEffect2c(prim + 0x14, 0x40400000u);   // 3.0
    SetULEffect2c(prim + 0x18, 0x40400000u);
    prim[5] = prim[4] = 0x20;
    prim[6] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 1u) << 5);
    SH_CALL(MapView_LinkPrimAt)(ULEffect2c(spark + 0xC), ULEffect2c(spark + 0x10), 0, 0x1C);
}

void EffectKind43_Inject() {
    BOF3_INJECT(EffectKind43_Run);
    BOF3_INJECT(EffectKind43_Start);
    BOF3_INJECT(EffectKind43_Gather);
    BOF3_INJECT(EffectKind43_Wait);
    BOF3_INJECT(EffectKind43_Fade);
    BOF3_INJECT(EffectKind6B_Run);
    BOF3_INJECT(EffectKind6B_Capture);
    BOF3_INJECT(EffectKind6B_Store);
    BOF3_INJECT(EffectKind6B_Scatter);
    BOF3_INJECT(EffectKind6B_Arm);
    BOF3_INJECT(EffectKind6B_Fall);
    BOF3_INJECT(EffectKind43_Setup);
    BOF3_INJECT(EffectKind43_AddSpark);
    BOF3_INJECT(EffectKind43_SparksRun);
    BOF3_INJECT(EffectKind43_SparkDraw);
    // DIV-0186: armed after the self-test, which compares ours with Capcom's at
    // 0x92EC80.
    static const std::uint8_t ash_was = 0, ash_is = 1;
    bof3::PatchBytes("AshRoom", AddressOfEffect2c(&effect_2c::g_ash_room), &ash_was, &ash_is, 1);
    bof3::Log("DIV-0186    kind 0x6B's particles in a room of %u (the original's holds 1,881): %s", at::kPixelRoom,
              effect_2c::g_ash_room ? "on" : "OFF (BOF3X_ORIGINAL=AshRoom)");
}
