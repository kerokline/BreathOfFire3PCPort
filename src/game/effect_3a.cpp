// Round thirteen group E3A (docs/effect_3a.md): the 48 functions of
// analysis/round13_cut.tsv's group E3A, 0x4801F0..0x4823C2, each read with
// capstone to its last instruction. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says (the spawners are chapter 8's run 11 and
// chapter 10's run 2, scena_sc7.cpp and scena_sc9b.cpp):
//
//   kind 0x60  a dispatcher only here (its states EffectKind60_Start..EffectKind60_SlideEnd are
//              catalog part 6 rows in no group), and the grey LINE_G2 its
//              states 1 and 2 draw between two projected points
//   kind 0x61  a party member (ObjTrio record +7) drawn off screen into VRAM
//              (0x340, 0x100) through a borrowed record of kind 0x3A, read
//              back, every opaque pixel made a particle, the particles flown
//              in (+6 = 0) or out (+6 not 0) as flickering TILE_1s
//   kind 0x62  24 orange rays shot from the record's point, each a LINE_G2
//              that runs out and draws in, until message 0x12 at the
//              chapter's step 0xC; then a widening orange ring (32 POLY_G4)
//   kind 0x64  a glow (32 POLY_G3) at the leader's point that grows, holds and
//              rises; sixteen shards (E4F's draw) and eight sparks; a trail
//              of two POLY_G4 and eight TILE_1 dots flown from it until the
//              chapter's count reaches 0x18; then a fade
//   kind 0x68  sixteen motes (16 POLY_G3 discs each) rising round the
//              record's point through a three-state table of their own; then
//              a wall of four POLY_F4 linked into the map that brightens
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. One
// divergence, forced: EffectKind64_DrawGlow's rim vertices take their depth
// from a stack word the original never writes; ours gives them the centre's
// (DIV-0068). Otherwise each is a faithful replacement; where the original
// jumps through a state table past its end, indexes ObjTrio, kind 0x61's
// frames or the effect records past their end, or loops for ever, ours aborts
// with a message (docs/effect_3a.md section 6). The x87 arithmetic is done in
// long double, which the compiler keeps on the x87 at the control word's
// precision, as the originals' fild / fadd / fsubr / fiadd / fstp chains are.
#include "game/effect_3a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3a::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t SW(U a) { return SW(At(a)); }
unsigned char& B(U a) { return At(a)[0]; }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSar(int a, U b, unsigned n) { return Sar(static_cast<U>(a) * b, n); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE; every operation
// in long double on the x87 at the game's control word, `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD I(std::int32_t v) { return static_cast<LD>(v); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }
const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table.
void Dispatch(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3a.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +9 down one; true when it reached 0 (Sprite_Current read for each access,
// as the originals).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// ObjTrio record `m` (an effect record's +7): a Fatal past the three, where
// the original reads the field objects after them.
unsigned char* Member(const char* who, unsigned m) {
    if (m >= at::kObjTrioCount)
        bof3::Fatal("%s: +7 is %u, past ObjTrio's three records - the original reads what follows "
                    "(docs/effect_3a.md section 6)",
                    who, m);
    return ObjTrio + at::kObjTrioStride * m;
}

// Kind 0x61's frame record (the variant byte 0x676266): a Fatal past its two,
// where the original reads kind 0x61's state table as a frame.
U Frame(const char* who) {
    const unsigned v = B(at::kVariant);
    if (v >= at::kFrameCount)
        bof3::Fatal("%s: the frame byte 0x676266 is %u, past the two frames at 0x654948 - the original reads what "
                    "follows (docs/effect_3a.md section 6)",
                    who, v);
    return at::kFrames + 8 * v;
}

// Effect_FindFree's record (answer & 0xFF, as the original widens it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= scenario_harness::at::kEffectCount)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_3a.md section 6)",
                    who, index);
    return Effect_Objects + index * scenario_harness::at::kEffectStride;
}

unsigned char* Ray(unsigned i) { return EffectKind30_Shards + i * at::kRayStride; }
unsigned char* Spark(unsigned i) { return EffectKind30_Shards + i * at::kSparkStride; }
unsigned char* Mote(unsigned i) { return EffectKind30_Shards + i * at::kMoteStride; }
unsigned char* Particle(U i) { return At(at::kParticles + i * at::kParticleStride); }

// The shade a wall gets from kind 0x68's frame word (`lea eax, [eax * 4 -
// 0x168]` on the whole register, compared as a word): the low byte below 0x100
// as a signed word, else 0xFF. The callee reads the byte.
U WallShade(U frame) {
    const U w = (frame * 4u - 0x168u) & 0xFFFFu;
    return static_cast<std::int16_t>(w) < 0x100 ? (w & 0xFFu) : 0xFFu;
}

}  // namespace

// ===========================================================================
// Kind 0x60: Effect_KindHandlers[0x60] (0x6554D0), EffectKind60_States (six)
// ===========================================================================

// original 0x4801F0 (hidden in 0x47FBE0): jmp [EffectKind60_States + +1 * 4],
// unbounded. Its states are catalog part 6 rows (EffectKind60_Start, _SlideBoth, _SlideEnd),
// 0x492750 twice and Effect_StateRelease.
extern "C" void __cdecl EffectKind60_Run(void) {
    Dispatch("EffectKind60_Run", EffectKind60_States, EffectKind60_States_count);
}

// original 0x4804C0: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0x100), committed
// to slot 1); a semi-transparent LINE_G2 from `from` projected (grey 0x40) to
// `to` projected (black), committed 0x24 to slot 1.
extern "C" void __cdecl EffectKind60_DrawLine(const long* from, const long* to) {
    DrawMode(1, 0x3C0, 0x100, 0, 1);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineG2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    SetUL(p + 8, UL(o));
    SetUL(p + 0xC, UL(o + 4));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    Copy12(p + 0x18, o);
    p[4] = 0x40;
    p[5] = 0x40;
    p[6] = 0x40;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x24);
}

// ===========================================================================
// Kind 0x61: Effect_KindHandlers[0x61] (0x6554D4), EffectKind61_States (five)
// ===========================================================================

// original 0x480590 (hidden in 0x4804C0): jmp [EffectKind61_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKind61_Run(void) {
    Dispatch("EffectKind61_Run", EffectKind61_States, EffectKind61_States_count);
}

// original 0x4805B0 (state 0): a free record (Effect_FindFree; none: +1 up and
// nothing else); the frame byte 0x676266 = ObjTrio record +7's +0x148 not 0;
// VRAM (0x340, 0x100, 0x80, 0x100) cleared (Gfx_ClearRect); the member's first
// 0x80 bytes copied into the record - in use, state 2, sub-state 0, kind 0x3A,
// its point the member's again, its screen position (+0x2E, +0x30) the frame's
// x and y, +0x25 Gpu_GetTPage(0, 0, 0x340, 0x100)'s low byte, +0x24 | 0x88,
// +0x26 0x80; with Sprite_Current the record, Sprite_SetAnimation(+8),
// Sprite_UpdateScreen and Effect_Release (the borrowed record freed at once);
// Sprite_Current back, +9 = 2 frames, +1 up.
extern "C" void __cdecl EffectKind61_Capture(void) {
    const unsigned char found = SH_CALL(Effect_FindFree)();
    if (found == 0xFF) {
        NextState();
        return;
    }
    const unsigned char* const member = Member("EffectKind61_Capture", S()[7]);
    B(at::kVariant) = member[at::kMemberFlag] != 0 ? 1 : 0;
    unsigned char* const e = NewRecord("EffectKind61_Capture", found);
    SH_CALL(Gfx_ClearRect)(0x340, 0x100, 0x80, 0x100);
    const unsigned char* const from = Member("EffectKind61_Capture", S()[7]);
    for (unsigned i = 0; i < scenario_harness::at::kEffectStride; i += 4) SetUL(e + i, UL(from + i));
    e[0] = 1;
    e[1] = 2;
    e[2] = 0;
    e[5] = 0x3A;
    SetUL(e + 0x34, UL(Member("EffectKind61_Capture", S()[7]) + 0x34));
    SetUL(e + 0x38, UL(Member("EffectKind61_Capture", S()[7]) + 0x38));
    SetUL(e + 0x3C, UL(Member("EffectKind61_Capture", S()[7]) + 0x3C));
    SetWord(e + 0x2E, Word(At(Frame("EffectKind61_Capture"))));
    SetWord(e + 0x30, Word(At(Frame("EffectKind61_Capture") + 2)));
    const U tp = SH_CALL(Gpu_GetTPage)(0, 0, 0x340, 0x100);
    e[0x25] = static_cast<unsigned char>(tp);
    e[0x26] = 0x80;
    e[0x24] = static_cast<unsigned char>(e[0x24] | 0x88);
    unsigned char* const self = S();
    Sprite_Current = e;
    SH_CALL(Sprite_SetAnimation)(e[8]);
    SH_CALL(Sprite_UpdateScreen)();
    SH_CALL(Effect_Release)();
    Sprite_Current = self;
    self[9] = 2;
    NextState();
}

// original 0x480730 (state 1): +9 down; at 0 the rectangle (0x340, 0x100, w, h)
// of the frame read back into 0x92BF80 (0x59E930) and +1 up.
extern "C" void __cdecl EffectKind61_Store(void) {
    if (!CountDown()) return;
    const U f = Frame("EffectKind61_Store");
    alignas(4) short rect[4] = {0x340, 0x100, static_cast<short>(Word(At(f + 4))), static_cast<short>(Word(At(f + 6)))};
    SH_AT(void (__cdecl*)(const short*, void*), at::kStoreImage)(rect, At(at::kShards));
    NextState();
}

// original 0x4807A0 (state 2): EffectGte_LoadMapCamera; the member's point
// (ObjTrio record +7, +0x34) projected (o); the count 0x676264 = 0; for each
// row below the frame's h and column below its w (s16; the row and column
// bytes) a read-back pixel that is not 0 becomes a particle at 0x92DF80 + 0x14 n:
// its colour +2, its screen point (o.x - x + column, o.y - y + row) at +4 / +8
// (floats); the count n; +1 up. A w or h above 0xFF never ends in the original
// (the row and column are bytes): ours aborts. The particles have no bound
// (docs/effect_3a.md section 7).
extern "C" void __cdecl EffectKind61_Scatter(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const s = S();
    const unsigned char* const member = Member("EffectKind61_Scatter", s[7]);
    alignas(4) unsigned char q[12];
    Copy12(q, member + 0x34);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o));
    const U f = Frame("EffectKind61_Scatter");
    U count = 0;
    SetWord(At(at::kParticleCount), 0);
    if (SW(f + 6) <= 0) {
        NextState();
        return;
    }
    if (SW(f + 6) > 0xFF || SW(f + 4) > 0xFF)
        bof3::Fatal("EffectKind61_Scatter: the frame is %d x %d - the original's row and column bytes never reach past "
                    "0xFF, it never ends (docs/effect_3a.md section 6)",
                    (int)SW(f + 4), (int)SW(f + 6));
    U pixel = at::kShards;
    unsigned char row = 0;
    do {
        const std::int32_t width = SW(f + 4);
        if (width > 0) {
            unsigned char column = 0;
            do {
                const U colour = Word(At(pixel));
                if (colour != 0) {
                    unsigned char* const p = Particle(count);
                    SetWord(p + 2, colour);
                    StF(p + 4, (F(o) - I(SW(f))) + I(column));
                    StF(p + 8, (F(o + 4) - I(SW(f + 2))) + I(row));
                    count = (count + 1) & 0xFFFFu;
                }
                pixel += 2;
                ++column;
            } while (static_cast<std::int32_t>(column) < width);
            SetWord(At(at::kParticleCount), count);
        }
        ++row;
    } while (static_cast<std::int32_t>(row) < SW(f + 6));
    NextState();
}

// original 0x480910 (state 3): a 16-byte pattern (i & 1); for each particle
// (the count a word): at every sixteenth, the pattern shuffled by 16 swaps of
// two Rand & 0xF places; its flag +0 the pattern's next byte, its speed +0x10 /
// +0x11 (Rand & 0x1F) - 0x10 each; with +6 = 0 its count +1 (Rand & 0x1E) + 0xA
// and its point moved back by speed * count (a float); else its count 0x28 -
// (Rand & 0x1E). +9 = 0x50, +1 up, Sound_PlayEffect(0x202).
extern "C" void __cdecl EffectKind61_Arm(void) {
    unsigned char pattern[16];
    for (unsigned i = 0; i < 16; ++i) pattern[i] = static_cast<unsigned char>(i & 1);
    U next = 0;
    for (U i = 0; (i & 0xFFFFu) < Word(At(at::kParticleCount)); ++i) {
        unsigned char* const p = Particle(i & 0xFFFFu);
        if ((i & 0xF) == 0) {
            for (unsigned k = 0; k < 16; ++k) {
                const unsigned a = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
                const unsigned b = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
                const unsigned char at_a = pattern[a];
                const unsigned char at_b = pattern[b];
                pattern[b] = at_a;
                pattern[a] = at_b;
            }
            next = 0;
        }
        p[0] = pattern[next & 0xFFFFu];
        ++next;
        p[0x10] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0x1Fu) - 0x10u);
        p[0x11] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0x1Fu) - 0x10u);
        if (S()[6] == 0) {
            const unsigned char n = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0x1Eu) + 0xAu);
            p[1] = n;
            const std::int32_t dx = static_cast<signed char>(p[0x10]) * static_cast<std::int32_t>(n);
            const std::int32_t dy = static_cast<signed char>(p[0x11]) * static_cast<std::int32_t>(n);
            StF(p + 4, F(p + 4) - I(dx));
            StF(p + 8, F(p + 8) - I(dy));
        } else {
            p[1] = static_cast<unsigned char>(0x28u - (static_cast<U>(SH_CALL(Rand)()) & 0x1Eu));
        }
    }
    S()[9] = 0x50;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x202);
}

// original 0x480A50 (state 4): each particle (the count a word) flagged +0:
// with +6 = 0, while its count +1 lasts, the count down and the point moved by
// its speed; with +6 not 0, the count down until it is 0, then moved each
// frame; drawn as a TILE_1 at its point (+4..+0xF copied), the colour the
// 15-bit pixel's channels (<< 3, >> 2, >> 7, each & 0xF8), committed to slot 2
// (0x14). Every particle's flag toggled (bit 0). +9 down, at 0 Effect_Release.
extern "C" void __cdecl EffectKind61_Twinkle(void) {
    for (U i = 0; (i & 0xFFFFu) < Word(At(at::kParticleCount)); ++i) {
        unsigned char* const p = Particle(i & 0xFFFFu);
        if (p[0] != 0) {
            const unsigned char out = S()[6];
            if (out == 0) {
                if (p[1] != 0) {
                    p[1] = static_cast<unsigned char>(p[1] - 1);
                    StF(p + 4, I(static_cast<signed char>(p[0x10])) + F(p + 4));
                    StF(p + 8, I(static_cast<signed char>(p[0x11])) + F(p + 8));
                }
            } else if (p[1] != 0) {
                p[1] = static_cast<unsigned char>(p[1] - 1);
            } else {
                StF(p + 4, I(static_cast<signed char>(p[0x10])) + F(p + 4));
                StF(p + 8, I(static_cast<signed char>(p[0x11])) + F(p + 8));
            }
            unsigned char* const prim = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(prim);
            Copy12(prim + 8, p + 4);
            prim[4] = static_cast<unsigned char>(p[2] << 3);
            prim[5] = static_cast<unsigned char>((Word(p + 2) >> 2) & 0xF8u);
            prim[6] = static_cast<unsigned char>((Word(p + 2) >> 7) & 0xF8u);
            SH_CALL(Gfx_CommitPrim)(2, 0x14);
        }
        p[0] = static_cast<unsigned char>(p[0] ^ 1);
    }
    if (CountDown()) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x62: Effect_KindHandlers[0x62] (0x6554D8), EffectKind62_States (three)
// ===========================================================================

// original 0x480B70 (hidden in 0x4804C0): jmp [EffectKind62_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKind62_Run(void) {
    Dispatch("EffectKind62_Run", EffectKind62_States, EffectKind62_States_count);
}

// original 0x480B90 (state 0): the rays cleared (EffectKind62_ClearRays); +9 =
// 0xFF, the wait +0xA = 0, the gap +6 = 0x10; +1 up; Sound_PlayEffect(0x204).
extern "C" void __cdecl EffectKind62_Start(void) {
    SH_CALL(EffectKind62_ClearRays)();
    S()[9] = 0xFF;
    S()[0xA] = 0;
    S()[6] = 0x10;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x204);
}

// original 0x480BD0 (state 1): when the wait +0xA has reached the gap +6, a ray
// (EffectKind62_SpawnRay), the gap down 2 unless 0, the wait 0; else the wait
// up. The rays stepped and drawn (EffectKind62_StepRays). When no message is
// opening (Field_Request not 2), message 0x12 is up (0x7DEE48) and the
// chapter's step is 0xC: the ring's radii +0xC = 0xA000 and +0x10 = 0xB000,
// their speeds +0x18 / +0x1C = 0, +9 = 0, +1 up.
extern "C" void __cdecl EffectKind62_Rays(void) {
    unsigned char* s = S();
    const unsigned char wait = s[0xA];
    if (wait == s[6]) {
        SH_CALL(EffectKind62_SpawnRay)();
        s = S();
        const unsigned char gap = s[6];
        if (gap != 0) {
            s[6] = static_cast<unsigned char>(gap - 2);
            s = S();
        }
        s[0xA] = 0;
    } else {
        s[0xA] = static_cast<unsigned char>(wait + 1);
    }
    SH_CALL(EffectKind62_StepRays)();
    if (Field_Request == 2) return;
    if (Word(At(at::kMessageWord)) != 0x12) return;
    if (B(at::kStep) != 0xC) return;
    SetUL(S() + 0xC, 0xA000);
    SetUL(S() + 0x10, 0xB000);
    SetUL(S() + 0x18, 0);
    SetUL(S() + 0x1C, 0);
    S()[9] = 0;
    NextState();
}

// original 0x480C70 (state 2): at +9 = 0x14 Sound_PlayEffect(0x205); +9 up to
// 0x15; the ring drawn (EffectKind62_DrawRing(+0x34, +0xC, +0x10)); the speeds
// +0x18 up 0x80 and +0x1C up 0x8C, the radii down by them, each held at 0; the
// rays stepped (EffectKind62_StepRays): none left and both radii 0 -
// Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind62_Blast(void) {
    unsigned char* s = S();
    if (s[9] == 0x14) {
        SH_CALL(Sound_PlayEffect)(0x205);
        s = S();
    }
    if (s[9] < 0x15) {
        s[9] = static_cast<unsigned char>(s[9] + 1);
        s = S();
    }
    SH_CALL(EffectKind62_DrawRing)(Point(s + 0x34), static_cast<int>(UL(s + 0xC)), static_cast<int>(UL(s + 0x10)));
    SetUL(S() + 0x18, UL(S() + 0x18) + 0x80u);
    SetUL(S() + 0x1C, UL(S() + 0x1C) + 0x8Cu);
    s = S();
    SetUL(s + 0xC, UL(s + 0xC) - UL(s + 0x18));
    s = S();
    SetUL(s + 0x10, UL(s + 0x10) - UL(s + 0x1C));
    s = S();
    if (Long(s + 0xC) < 0) {
        SetUL(s + 0xC, 0);
        s = S();
    }
    if (Long(s + 0x10) < 0) SetUL(s + 0x10, 0);
    if (SH_CALL(EffectKind62_StepRays)() != 0) return;
    s = S();
    if (UL(s + 0xC) != 0 || UL(s + 0x10) != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x4812B0: the 24 rays of 0x38 at 0x92BF80 out of use.
extern "C" void __cdecl EffectKind62_ClearRays(void) {
    for (unsigned i = 0; i < at::kRays; ++i) Ray(i)[0] = 0;
}

// original 0x4812D0: the first free ray of the 24 (none: nothing): in use,
// phase 0, count 8; an angle a = Rand & 0xFFF and a length r = ((Rand & 0xFFF)
// + 0x800) << 5; both shades +3 / +4 = r / 0x300 (the compiler's signed divide
// of r << 8 by 0x30000); both ends (+8, +0x18) the record's point plus (cos a,
// sin a) * r sar 12 on x and z, the height the record's; the speed +0x28 /
// +0x2C = -(cos a, sin a) << 1, +0x30 = 0.
extern "C" void __cdecl EffectKind62_SpawnRay(void) {
    unsigned char* ray = nullptr;
    for (unsigned i = 0; i < at::kRays; ++i)
        if (Ray(i)[0] == 0) {
            ray = Ray(i);
            break;
        }
    if (ray == nullptr) return;
    ray[0] = 1;
    ray[1] = 0;
    ray[2] = 8;
    const U angle = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U r = ((static_cast<U>(SH_CALL(Rand)()) & 0xFFFu) + 0x800u) << 5;
    // mov eax, 0x2AAAAAAB; imul (r << 8); sar edx, 0xF; edx += edx >> 31
    const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(r << 8)) * 0x2AAAAAABLL;
    U q = Sar(static_cast<U>(static_cast<std::uint64_t>(product) >> 32), 0xF);
    q += q >> 31;
    ray[4] = static_cast<unsigned char>(q);
    ray[3] = static_cast<unsigned char>(q);
    U v = MulSar(SH_CALL(Math_Cos)(static_cast<int>(angle)), r, 12) + UL(S() + 0x34);
    SetUL(ray + 0x18, v);
    SetUL(ray + 8, v);
    v = MulSar(SH_CALL(Math_Sin)(static_cast<int>(angle)), r, 12);
    unsigned char* const s = S();
    v += UL(s + 0x38);
    SetUL(ray + 0x1C, v);
    SetUL(ray + 0xC, v);
    SetUL(ray + 0x20, UL(s + 0x3C));
    SetUL(ray + 0x10, UL(s + 0x3C));
    SetUL(ray + 0x28, (0u - static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)))) << 1);
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
    SetUL(ray + 0x30, 0);
    SetUL(ray + 0x2C, (0u - sin) << 1);
}

// original 0x4813B0: EffectGte_LoadMapCamera; a draw mode (Gpu_GetTPage(0, 1,
// 0x2C0, 0x100), committed to slot 2); each ray in use drawn
// (EffectKind62_DrawRay), then by its phase - 0: its first end moved by the
// speed, its shade +3 down 10; 1: its second end moved, its shade +4 down 10 -
// its count down, at 0 phase 0 goes to 1 with the count 8 and phase 1 ends the
// ray. Answers 1 when a ray was in use, else 0.
extern "C" unsigned char __cdecl EffectKind62_StepRays(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    DrawMode(1, 0x2C0, 0x100, 0, 2);
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kRays; ++i) {
        unsigned char* const ray = Ray(i);
        if (ray[0] == 0) continue;
        any = 1;
        SH_CALL(EffectKind62_DrawRay)(ray);
        if (ray[1] == 0) {
            SetUL(ray + 8, UL(ray + 8) + UL(ray + 0x28));
            SetUL(ray + 0xC, UL(ray + 0xC) + UL(ray + 0x2C));
            SetUL(ray + 0x10, UL(ray + 0x10) + UL(ray + 0x30));
            ray[3] = static_cast<unsigned char>(ray[3] - 10);
            ray[2] = static_cast<unsigned char>(ray[2] - 1);
            if (ray[2] == 0) {
                ray[2] = 8;
                ray[1] = static_cast<unsigned char>(ray[1] + 1);
            }
        } else if (ray[1] == 1) {
            SetUL(ray + 0x18, UL(ray + 0x18) + UL(ray + 0x28));
            SetUL(ray + 0x1C, UL(ray + 0x1C) + UL(ray + 0x2C));
            SetUL(ray + 0x20, UL(ray + 0x20) + UL(ray + 0x30));
            ray[4] = static_cast<unsigned char>(ray[4] - 10);
            ray[2] = static_cast<unsigned char>(ray[2] - 1);
            if (ray[2] == 0) ray[0] = 0;
        }
    }
    return any;
}

// original 0x4814B0: a semi-transparent LINE_G2 from the ray's first end
// projected, shaded (+3, +3 >> 1, 0), to its second, shaded (+4, +4 >> 1, 0);
// committed 0x24 to slot 2.
extern "C" void __cdecl EffectKind62_DrawRay(unsigned char* ray) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineG2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(ray + 8), Out(o));
    Copy12(p + 8, o);
    SH_CALL(EffectGte_ProjectPoint)(Point(ray + 0x18), Out(o));
    Copy12(p + 0x18, o);
    p[4] = ray[3];
    p[5] = static_cast<unsigned char>(ray[3] >> 1);
    p[6] = 0;
    p[0x14] = ray[4];
    p[0x15] = static_cast<unsigned char>(ray[4] >> 1);
    p[0x16] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x24);
}

// original 0x481550: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), committed
// to slot 2); EffectGte_LoadMapCamera; a ring of 32 semi-transparent POLY_G4
// round `point` between the radii inner << 4 (orange 0xFF, 0x7F, 0) and outer
// << 4 (black), (cos, sin) * r sar 8 at 0x80 steps from the angle 0x80 (the
// first edge at angle 0 as x + r << 4), the height point's; each committed 0x44
// to slot 2. Answers 0 in al.
extern "C" unsigned char __cdecl EffectKind62_DrawRing(const long* point, int inner, int outer) {
    DrawMode(1, 0x2C0, 0x100, 0, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char* const pt = Bytes(point);
    const U a = static_cast<U>(inner), b = static_cast<U>(outer);
    alignas(4) unsigned char q[12];
    SetUL(q, (a << 4) + UL(pt));
    SetUL(q + 4, UL(pt + 4));
    SetUL(q + 8, UL(pt + 8));
    alignas(4) unsigned char o1[12], o2[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o1));
    SetUL(q, (b << 4) + UL(pt));
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o2));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        angle += 0x80;
        const int t = static_cast<int>(angle & 0xFFFFu);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o1);
        Copy12(p + 0x18, o2);
        U v = MulSar(SH_CALL(Math_Cos)(t), a, 8);
        SetUL(q, v + UL(pt));
        v = MulSar(SH_CALL(Math_Sin)(t), a, 8);
        SetUL(q + 4, v + UL(pt + 4));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o1));
        v = MulSar(SH_CALL(Math_Cos)(t), b, 8);
        SetUL(q, v + UL(pt));
        v = MulSar(SH_CALL(Math_Sin)(t), b, 8);
        SetUL(q + 4, v + UL(pt + 4));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o2));
        Copy12(p + 0x28, o1);
        Copy12(p + 0x38, o2);
        p[4] = 0xFF;
        p[5] = 0x7F;
        p[6] = 0;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0xFF;
        p[0x25] = 0x7F;
        p[0x26] = 0;
        p[0x34] = 0;
        p[0x35] = 0;
        p[0x36] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x44);
    }
    return 0;
}

// ===========================================================================
// Kind 0x64: Effect_KindHandlers[0x64] (0x6554E0), EffectKind64_States (nine)
// ===========================================================================

// original 0x480D40 (hidden in 0x4804C0): jmp [EffectKind64_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKind64_Run(void) {
    Dispatch("EffectKind64_Run", EffectKind64_States, EffectKind64_States_count);
}

// original 0x480D60 (state 0; also state 0 of the table 0x655320 that group
// E4F's dispatcher 0x493550 indexes): the glow's point +0x64 / +0x68 / +0x6C the
// leader's (ObjTrio +0x34..), its height 0x80 up; the size +0x2E = 0; +9 = 8;
// +1 up.
extern "C" void __cdecl EffectKind64_Start(void) {
    SetUL(S() + 0x64, UL(at::kLeaderPoint));
    SetUL(S() + 0x68, UL(at::kLeaderPoint + 4));
    SetUL(S() + 0x6C, UL(at::kLeaderPoint + 8) + 0x800000u);
    SetWord(S() + 0x2E, 0);
    S()[9] = 8;
    NextState();
}

// original 0x480DB0 (state 1): the size +0x2E up 0x20; the glow drawn
// (EffectKind64_DrawGlow(+0x64, +0x2E, 6)); +9 down, at 0 +9 = 0x80, +1 up and
// Sound_PlayEffect(0x209).
extern "C" void __cdecl EffectKind64_Grow(void) {
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0x20u);
    unsigned char* const s = S();
    SH_CALL(EffectKind64_DrawGlow)(Point(s + 0x64), Word(s + 0x2E), 6);
    if (!CountDown()) return;
    S()[9] = 0x80;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x480E10 (state 2): the glow drawn; +9 down, at 0 +9 = 0x20, +1 up.
extern "C" void __cdecl EffectKind64_Hold(void) {
    unsigned char* const s = S();
    SH_CALL(EffectKind64_DrawGlow)(Point(s + 0x64), Word(s + 0x2E), 6);
    if (!CountDown()) return;
    S()[9] = 0x20;
    NextState();
}

// original 0x480E50 (state 3): the glow's height up 0xC; its size down 6; the
// glow drawn; +9 down, at 0: +9 = 0x20, +6 = 0, +1 up; the sparks cleared
// (EffectKind64_ClearSparks); the record's point +0x34.. the glow's; the 16
// shards at 0x92C040 started (EffectKind64_InitShard); Sound_PlayEffect(0x207).
extern "C" void __cdecl EffectKind64_Rise(void) {
    SetUL(S() + 0x6C, UL(S() + 0x6C) + 0xC0000u);
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0xFFFAu);
    unsigned char* const s = S();
    SH_CALL(EffectKind64_DrawGlow)(Point(s + 0x64), Word(s + 0x2E), 6);
    if (!CountDown()) return;
    S()[9] = 0x20;
    S()[6] = 0;
    NextState();
    SH_CALL(EffectKind64_ClearSparks)();
    SetUL(S() + 0x34, UL(S() + 0x64));
    SetUL(S() + 0x38, UL(S() + 0x68));
    SetUL(S() + 0x3C, UL(S() + 0x6C));
    for (unsigned i = 0; i < at::kShardCount64; ++i)
        SH_CALL(EffectKind64_InitShard)(At(at::kShards64 + i * at::kShardStride64));
    SH_CALL(Sound_PlayEffect)(0x207);
}

namespace {
// The opening of kind 0x64's states 4..7: the frame +6 up, and on every fourth
// (its value before, & 3 = 0) a spark (group E4F's 0x493B50).
void SparkTick() {
    unsigned char* const s = S();
    const unsigned char frame = s[6];
    s[6] = static_cast<unsigned char>(frame + 1);
    if ((frame & 3) == 0) SH_AT(void (__cdecl*)(void), at::kSparkSpawn)();
}
}  // namespace

// original 0x480F10 (state 4): a spark every fourth frame; the shards and the
// sparks drawn (EffectKind64_DrawShards, EffectKind64_StepSparks); +9 down, at
// 0: the trail's head +0x18.. the glow's point, its speeds +0xC = -0x1000 and
// +0x10 = -0x400, +1 up.
extern "C" void __cdecl EffectKind64_Burst(void) {
    SparkTick();
    SH_CALL(EffectKind64_DrawShards)();
    SH_CALL(EffectKind64_StepSparks)();
    if (!CountDown()) return;
    unsigned char* const s = S();
    SetUL(s + 0x18, UL(s + 0x64));
    SetUL(S() + 0x1C, UL(S() + 0x68));
    SetUL(S() + 0x20, UL(S() + 0x6C));
    SetUL(S() + 0xC, 0xFFFFF000u);
    SetUL(S() + 0x10, 0xFFFFFC00u);
    NextState();
}

// original 0x480F90 (state 5): the same spark and draws; +9 = 0x20, +1 up.
extern "C" void __cdecl EffectKind64_Launch(void) {
    SparkTick();
    SH_CALL(EffectKind64_DrawShards)();
    SH_CALL(EffectKind64_StepSparks)();
    S()[9] = 0x20;
    NextState();
}

// original 0x480FD0 (state 6): the spark; the sparks and the shards drawn; the
// trail (EffectKind64_DrawTrail(+0x18, +0x34, 0x40)); the speeds +0xC down
// 0x800 and +0x10 down 0x200, the head +0x18 / +0x1C moved by them; the shade
// +3 down 4; +9 down, at 0 +9 = 0x80, +1 up.
extern "C" void __cdecl EffectKind64_Fly(void) {
    SparkTick();
    SH_CALL(EffectKind64_StepSparks)();
    SH_CALL(EffectKind64_DrawShards)();
    unsigned char* const s = S();
    SH_CALL(EffectKind64_DrawTrail)(Point(s + 0x18), Point(s + 0x34), 0x40);
    SetUL(S() + 0xC, UL(S() + 0xC) + 0xFFFFF800u);
    SetUL(S() + 0x10, UL(S() + 0x10) + 0xFFFFFE00u);
    unsigned char* t = S();
    SetUL(t + 0x18, UL(t + 0x18) + UL(t + 0xC));
    t = S();
    SetUL(t + 0x1C, UL(t + 0x1C) + UL(t + 0x10));
    S()[3] = static_cast<unsigned char>(S()[3] + 0xFC);
    if (!CountDown()) return;
    S()[9] = 0x80;
    NextState();
}

// original 0x481090 (state 7): the spark; the sparks and the shards drawn; the
// trail (+0x18, +0x34, 0x40); when the chapter's count 0x903848 is 0x18 the
// shade +3 = 0x80, +9 = 8, +1 up and Sound_PlayEffect(0x208).
extern "C" void __cdecl EffectKind64_FlyWait(void) {
    SparkTick();
    SH_CALL(EffectKind64_StepSparks)();
    SH_CALL(EffectKind64_DrawShards)();
    unsigned char* const s = S();
    SH_CALL(EffectKind64_DrawTrail)(Point(s + 0x18), Point(s + 0x34), 0x40);
    if (B(at::kCounter) != 0x18) return;
    S()[3] = 0x80;
    S()[9] = 8;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x208);
}

// original 0x481100 (state 8): the size +0x2E down 0x10; the shade +3 down 8;
// the sparks drawn; the trail (+0x18, +0x34, the shade); +9 down, at 0
// Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind64_Fade(void) {
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0xFFF0u);
    S()[3] = static_cast<unsigned char>(S()[3] + 0xF8);
    SH_CALL(EffectKind64_StepSparks)();
    unsigned char* const s = S();
    SH_CALL(EffectKind64_DrawTrail)(Point(s + 0x18), Point(s + 0x34), s[3]);
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x481740: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), dtd 1,
// committed to slot 2); EffectGte_LoadMapCamera; `point` projected (o) and
// its size {size, size} projected (EffectGte_ProjectSize); the radius r = that
// size's first word + (Frame_Counter & 1), a word; a fan of 32 semi-transparent
// POLY_G3 round o - the centre in the colour bits' shades ((c & 0xFC) << 5,
// (c & 0xFE) << 6, c << 7, each a byte), the rim black at o + (cos, sin) * r
// sar 12 from the angle 0x80 by 0x80 (the first rim point o.x + r); each
// committed 0x34 to slot 2. The rim's depth is a stack word the original never
// writes; ours gives it the centre's, o's (DIV-0068).
extern "C" void __cdecl EffectKind64_DrawGlow(const long* point, unsigned size, unsigned colour) {
    DrawMode(1, 0x2C0, 0x100, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(point, Out(o));
    alignas(4) short sz[2] = {static_cast<short>(size), static_cast<short>(size)};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(point, sz, r);
    const std::int32_t radius = static_cast<std::int16_t>(static_cast<U>(r[0]) + (Frame_Counter & 1u));
    const unsigned char c = static_cast<unsigned char>(colour);
    const unsigned char red = static_cast<unsigned char>((c & 0xFC) << 5);
    const unsigned char green = static_cast<unsigned char>((c & 0xFE) << 6);
    const unsigned char blue = static_cast<unsigned char>(c << 7);
    alignas(4) unsigned char rim[12];
    StF(rim, I(radius) + F(o));
    SetUL(rim + 4, UL(o + 4));
    SetUL(rim + 8, UL(o + 8));  // DIV-0068: the original's rim depth is an unwritten stack word
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        angle += 0x80;
        const int t = static_cast<int>(angle & 0xFFFFu);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o);
        Copy12(p + 0x18, rim);
        const U dx = MulSar(SH_CALL(Math_Cos)(t), static_cast<U>(radius), 12);
        StF(rim, I(static_cast<std::int32_t>(dx)) + F(o));
        const U dy = MulSar(SH_CALL(Math_Sin)(t), static_cast<U>(radius), 12);
        SetUL(p + 0x28, UL(rim));
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[0x25] = 0;
        StF(rim + 4, I(static_cast<std::int32_t>(dy)) + F(o + 4));
        p[0x26] = 0;
        SetUL(p + 0x2C, UL(rim + 4));
        p[5] = green;
        SetUL(p + 0x30, UL(rim + 8));
        p[4] = red;
        p[6] = blue;
        SH_CALL(Gfx_CommitPrim)(2, 0x34);
    }
}

// original 0x481910: two semi-transparent POLY_G4 along the trail from `from`
// projected (a) to `to` projected (b) - the first (a + (h, -h), a, b + (h,
// -h), b) with a and b shaded (shade, shade, 0), the second (a, a + (-h, h),
// b, b + (-h, h)) with the offset corners black and a, b shaded, h the float at
// 0x5C41B8 - each committed 0x44 to slot 2; then up to eight TILE_1 dots,
// (shade, shade, 0), from `to` less (Frame_Counter & 0xF) << 12 on x and << 10
// on z, stepping 0x10000 and 0x4000 back, while x is not below from's.
extern "C" void __cdecl EffectKind64_DrawTrail(const long* from, const long* to, unsigned shade) {
    const unsigned char* const a = Bytes(from);
    const unsigned char* const b = Bytes(to);
    const LD half = F(At(at::kHalf));
    alignas(4) unsigned char o[12];
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    StF(p + 8, F(o) + half);
    StF(p + 0xC, F(o + 4) - half);
    SetUL(p + 0x18, UL(o));
    SetUL(p + 0x1C, UL(o + 4));
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    const unsigned char k = static_cast<unsigned char>(shade);
    StF(p + 0x28, F(o) + half);
    StF(p + 0x2C, F(o + 4) - half);
    SetUL(p + 0x38, UL(o));
    SetUL(p + 0x3C, UL(o + 4));
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = k;
    p[0x15] = k;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = k;
    p[0x35] = k;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    SetUL(p + 8, UL(o));
    SetUL(p + 0xC, UL(o + 4));
    StF(p + 0x18, F(o) - half);
    StF(p + 0x1C, F(o + 4) + half);
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    SetUL(p + 0x28, UL(o));
    SetUL(p + 0x2C, UL(o + 4));
    StF(p + 0x38, F(o) - half);
    StF(p + 0x3C, F(o + 4) + half);
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = k;
    p[5] = k;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = k;
    p[0x25] = k;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    const U f = Frame_Counter & 0xFu;
    alignas(4) unsigned char q[12];
    SetUL(q, UL(b) - (f << 12));
    SetUL(q + 4, UL(b + 4) - (f << 10));
    SetUL(q + 8, UL(b + 8));
    for (unsigned char dot = 0; dot < 8; ++dot) {
        if (Long(q) < Long(a)) return;
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o));
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o);
        p[4] = k;
        p[5] = k;
        p[6] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x14);
        SetUL(q, UL(q) - 0x10000u);
        SetUL(q + 4, UL(q + 4) - 0x4000u);
    }
}

// original 0x482050: the eight sparks of 0x18 at 0x92BF80 out of use.
extern "C" void __cdecl EffectKind64_ClearSparks(void) {
    for (unsigned i = 0; i < at::kSparks; ++i) Spark(i)[0] = 0;
}

// original 0x482070: EffectGte_LoadMapCamera; each spark in use: its size +0x14
// down 0x10, its life +2 down (at 0 out of use), drawn (EffectKind64_DrawSpark).
// Answers 1 when a spark was in use, else 0.
extern "C" unsigned char __cdecl EffectKind64_StepSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] == 0) continue;
        const unsigned char life = k[2];
        SetWord(k + 0x14, Word(k + 0x14) + 0xFFF0u);
        any = 1;
        k[2] = static_cast<unsigned char>(life - 1);
        if (k[2] == 0) k[0] = 0;
        SH_CALL(EffectKind64_DrawSpark)(k);
    }
    return any;
}

// original 0x4820C0: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1,
// committed to slot 2); the spark's point +4 projected (o) and its size {+0x14,
// a word the original never writes} projected (EffectGte_ProjectSize, which
// reads the first); r = that size's first word + (Frame_Counter & 1); a disc of
// 32 semi-transparent POLY_G3 round o, the centre (+0x16, +0x16, 0), the rim
// (+0x17, +0x17, 0) at (cos, sin) * r sar 12, 0x80 apart; each committed 0x34
// to slot 2.
extern "C" void __cdecl EffectKind64_DrawSpark(unsigned char* spark) {
    DrawMode(1, 0x3C0, 0, 1, 2);
    alignas(4) short sz[2] = {static_cast<short>(Word(spark + 0x14)), 0};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(Point(spark + 4), sz, r);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 4), Out(o));
    const std::int32_t radius = static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        U d = MulSar(SH_CALL(Math_Cos)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        StF(p + 0x18, I(static_cast<std::int32_t>(d)) + F(o));
        d = MulSar(SH_CALL(Math_Sin)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        angle += 0x80;
        StF(p + 0x1C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        d = MulSar(SH_CALL(Math_Cos)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        StF(p + 0x28, I(static_cast<std::int32_t>(d)) + F(o));
        d = MulSar(SH_CALL(Math_Sin)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        StF(p + 0x2C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        SetUL(p + 0x30, UL(o + 8));
        SetUL(p + 0x20, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        p[5] = spark[0x16];
        p[4] = spark[0x16];
        p[6] = 0;
        p[0x25] = spark[0x17];
        p[0x24] = spark[0x17];
        p[0x15] = spark[0x17];
        p[0x14] = spark[0x17];
        p[0x26] = 0;
        p[0x16] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x34);
    }
}

// original 0x482240: the shard's point +0..+8 the record's (+0x34..); three Rand
// angles x = & 0xFFF, y = (& 0x7FF) - 0x400, z = & 0xFFF; its two edge vectors
// +0x10 = (cos 0x10, sin 0x10, 0) and +0x18 = (cos -0x10, sin -0x10, 0) as
// words, turned by the matrix EffectGte_SetDiagonalOne, Gte_RotMatrixX (x),
// Gte_RotMatrixY (-y), Gte_RotMatrixZ (z) make (0x5A7C70, in place); its
// shade +0x2A = 0x20, its speed +0x28 = (Rand & 2) + 3, its angles +0x20..+0x25
// = 0.
extern "C" void __cdecl EffectKind64_InitShard(unsigned char* shard) {
    SetUL(shard, UL(S() + 0x34));
    SetUL(shard + 4, UL(S() + 0x38));
    SetUL(shard + 8, UL(S() + 0x3C));
    const U x = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U y = (static_cast<U>(SH_CALL(Rand)()) & 0x7FFu) - 0x400u;
    const U z = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(shard + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x10)));
    SetWord(shard + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x10)));
    SetWord(shard + 0x14, 0);
    SetWord(shard + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x10)));
    SetWord(shard + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x10)));
    SetWord(shard + 0x1C, 0);
    alignas(4) short m[16];
    SH_CALL(EffectGte_SetDiagonalOne)(m);
    SH_CALL(Gte_RotMatrixX)(static_cast<std::int16_t>(x), m);
    SH_CALL(Gte_RotMatrixY)(-static_cast<std::int32_t>(static_cast<std::int16_t>(y)), m);
    SH_CALL(Gte_RotMatrixZ)(static_cast<std::int16_t>(z), m);
    using Turn = void (__cdecl*)(const short*, short*, short*);
    SH_AT(Turn, at::kMatrixVector)(m, reinterpret_cast<short*>(shard + 0x10), reinterpret_cast<short*>(shard + 0x10));
    SH_AT(Turn, at::kMatrixVector)(m, reinterpret_cast<short*>(shard + 0x18), reinterpret_cast<short*>(shard + 0x18));
    const U speed = (static_cast<U>(SH_CALL(Rand)()) & 2u) + 3u;
    SetWord(shard + 0x2A, 0x20);
    SetWord(shard + 0x28, speed);
    SetWord(shard + 0x20, 0);
    SetWord(shard + 0x22, 0);
    SetWord(shard + 0x24, 0);
}

// original 0x482360: a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1,
// committed to slot 2); EffectGte_LoadMapCamera; each of the 16 shards at
// 0x92C040 drawn (group E4F's 0x493C60) and its angle +0x24 up 0x10.
extern "C" void __cdecl EffectKind64_DrawShards(void) {
    DrawMode(1, 0x380, 0x100, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kShardCount64; ++i) {
        unsigned char* const shard = At(at::kShards64 + i * at::kShardStride64);
        SH_AT(void (__cdecl*)(unsigned char*), at::kShardDraw)(shard);
        SetWord(shard + 0x24, Word(shard + 0x24) + 0x10u);
    }
}

// ===========================================================================
// Kind 0x68: Effect_KindHandlers[0x68] (0x6554F0), EffectKind68_States (four)
// and the motes' EffectKind68_MoteStates (three, called with the mote)
// ===========================================================================

// original 0x481150 (hidden in 0x4804C0): jmp [EffectKind68_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKind68_Run(void) {
    Dispatch("EffectKind68_Run", EffectKind68_States, EffectKind68_States_count);
}

// original 0x481170 (state 0): the motes cleared (EffectKind68_ClearMotes); the
// count +0x2E = 0x40, the frame +0x30 = 0; +1 up; Sound_PlayEffect(0x200).
extern "C" void __cdecl EffectKind68_Start(void) {
    SH_CALL(EffectKind68_ClearMotes)();
    SetWord(S() + 0x2E, 0x40);
    SetWord(S() + 0x30, 0);
    NextState();
    SH_CALL(Sound_PlayEffect)(0x200);
}

// original 0x4811A0 (state 1): the frame +0x30 up; when the count's low byte
// & 3 is 0, a free mote (EffectKind68_FindMote) started (EffectKind68_InitMote);
// the motes stepped (EffectKind68_StepMotes); the count down, at 0 +9 = 0 and
// +1 up.
extern "C" void __cdecl EffectKind68_Rise(void) {
    SetWord(S() + 0x30, Word(S() + 0x30) + 1u);
    if ((S()[0x2E] & 3) == 0) {
        unsigned char* const mote = SH_CALL(EffectKind68_FindMote)();
        if (mote != nullptr) SH_CALL(EffectKind68_InitMote)(mote);
    }
    SH_CALL(EffectKind68_StepMotes)();
    SetWord(S() + 0x2E, Word(S() + 0x2E) - 1u);
    unsigned char* const s = S();
    if (Word(s + 0x2E) != 0) return;
    s[9] = 0;
    NextState();
}

// original 0x4811F0 (state 2): at the frame 0x5A Sound_PlayEffect(0x201); the
// frame up; past 0x5A (a signed word) the wall drawn (EffectKind68_DrawWall,
// its shade the frame * 4 - 0x168, 0xFF from 0x100); the motes stepped: none
// left - the count +0x2E = 0x5A, +1 up.
extern "C" void __cdecl EffectKind68_Wall(void) {
    unsigned char* s = S();
    if (Word(s + 0x30) == 0x5A) {
        SH_CALL(Sound_PlayEffect)(0x201);
        s = S();
    }
    SetWord(s + 0x30, Word(s + 0x30) + 1u);
    const U frame = Word(S() + 0x30);
    if (static_cast<std::int16_t>(frame) > 0x5A) SH_CALL(EffectKind68_DrawWall)(WallShade(frame));
    if (SH_CALL(EffectKind68_StepMotes)() != 0) return;
    SetWord(S() + 0x2E, 0x5A);
    NextState();
}

// original 0x481260 (state 3): the frame up; past 0x5A the wall drawn as in
// state 2; the count down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind68_End(void) {
    SetWord(S() + 0x30, Word(S() + 0x30) + 1u);
    unsigned char* s = S();
    const U frame = Word(s + 0x30);
    if (static_cast<std::int16_t>(frame) > 0x5A) {
        SH_CALL(EffectKind68_DrawWall)(WallShade(frame));
        s = S();
    }
    SetWord(s + 0x2E, Word(s + 0x2E) - 1u);
    if (Word(S() + 0x2E) == 0) SH_CALL(Effect_Release)();
}

// original 0x481B80: the 16 motes of 0x1C at 0x92BF80 out of use; the two
// flags 0x676261 and 0x676260 cleared.
extern "C" void __cdecl EffectKind68_ClearMotes(void) {
    for (unsigned i = 0; i < at::kMotes; ++i) Mote(i)[0] = 0;
    B(at::kMoteFlagA) = 0;
    B(at::kMoteFlagB) = 0;
}

// original 0x481BB0: the first free mote of the 16, or null.
extern "C" unsigned char* __cdecl EffectKind68_FindMote(void) {
    for (unsigned i = 0; i < at::kMotes; ++i)
        if (Mote(i)[0] == 0) return Mote(i);
    return nullptr;
}

// original 0x481BD0: the mote in use, state 0, count 8, shade 0, speed +8 = 0,
// reload +4 = 0x20; its point +0xC.. the record's, x moved by ((Rand & 0xFF) -
// 0x80) << 10 and the height up (Rand % 0x300) << 16 (a signed remainder).
extern "C" void __cdecl EffectKind68_InitMote(unsigned char* mote) {
    mote[0] = 1;
    mote[1] = 0;
    mote[2] = 8;
    mote[3] = 0;
    SetUL(mote + 8, 0);
    mote[4] = 0x20;
    const U spread = ((static_cast<U>(SH_CALL(Rand)()) & 0xFFu) - 0x80u) << 10;
    SetUL(mote + 0xC, spread + UL(S() + 0x34));
    SetUL(mote + 0x10, UL(S() + 0x38));
    const std::int32_t rise = static_cast<std::int32_t>(SH_CALL(Rand)()) % 0x300;
    SetUL(mote + 0x14, (static_cast<U>(rise) << 16) + UL(S() + 0x3C));
}

// original 0x481C40: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1,
// committed to slot 2); EffectGte_LoadMapCamera; each mote in use run through
// EffectKind68_MoteStates by its +1 (a call with the mote, unbounded) and
// drawn (EffectKind68_DrawMote). Answers 1 when a mote was in use, else 0.
extern "C" unsigned char __cdecl EffectKind68_StepMotes(void) {
    unsigned char any = 0;
    DrawMode(1, 0x3C0, 0, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kMotes; ++i) {
        unsigned char* const mote = Mote(i);
        if (mote[0] == 0) continue;
        const unsigned state = mote[1];
        if (state >= EffectKind68_MoteStates_count)
            bof3::Fatal("EffectKind68_StepMotes: mote %u's state +1 is %u, past the %u entries of EffectKind68_MoteStates - "
                        "the original calls through the dword after (docs/effect_3a.md section 6)",
                        i, state, EffectKind68_MoteStates_count);
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(
            static_cast<std::uintptr_t>(UL(reinterpret_cast<const unsigned char*>(EffectKind68_MoteStates) + 4 * state)))(mote);
        SH_CALL(EffectKind68_DrawMote)(mote);
        any = 1;
    }
    return any;
}

// original 0x481CC0: the mote's point +0xC projected (o) and its size {0x18, a
// word the original never writes} projected (EffectGte_ProjectSize reads the
// first); r = that size's first word + (Frame_Counter & 1); a disc of 16
// semi-transparent POLY_G3 round o, the centre in the shade +3 (all three
// channels), the rim black at (cos, sin) * r sar 12, 0x100 apart; each
// committed 0x34 to slot 2.
extern "C" void __cdecl EffectKind68_DrawMote(unsigned char* mote) {
    alignas(4) short sz[2] = {0x18, 0};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(Point(mote + 0xC), sz, r);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(mote + 0xC), Out(o));
    const std::int32_t radius = static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u));
    U angle = 0;
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        U d = MulSar(SH_CALL(Math_Cos)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        StF(p + 0x18, I(static_cast<std::int32_t>(d)) + F(o));
        d = MulSar(SH_CALL(Math_Sin)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        angle += 0x100;
        StF(p + 0x1C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        d = MulSar(SH_CALL(Math_Cos)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        StF(p + 0x28, I(static_cast<std::int32_t>(d)) + F(o));
        d = MulSar(SH_CALL(Math_Sin)(static_cast<int>(angle)), static_cast<U>(radius), 12);
        StF(p + 0x2C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        SetUL(p + 0x30, UL(o + 8));
        SetUL(p + 0x20, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        const unsigned char shade = mote[3];
        p[6] = shade;
        p[5] = shade;
        p[4] = shade;
        p[0x26] = 0;
        p[0x25] = 0;
        p[0x24] = 0;
        p[0x16] = 0;
        p[0x15] = 0;
        p[0x14] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x34);
    }
}

// original 0x481E00 (EffectKind68_MoteStates[0], hidden in 0x481CC0): the
// shade +3 up 0x10; the count +2 down, at 0 the count its reload +4, +1 up and
// the flag 0x676261 set.
extern "C" void __cdecl EffectKind68_MoteGlow(unsigned char* mote) {
    mote[3] = static_cast<unsigned char>(mote[3] + 0x10);
    mote[2] = static_cast<unsigned char>(mote[2] - 1);
    if (mote[2] != 0) return;
    mote[2] = mote[4];
    mote[1] = static_cast<unsigned char>(mote[1] + 1);
    if (B(at::kMoteFlagA) == 0) B(at::kMoteFlagA) = 1;
}

// original 0x481E40 (EffectKind68_MoteStates[1]): the count down, at 0 the
// count 0x10 and +1 up.
extern "C" void __cdecl EffectKind68_MoteHold(unsigned char* mote) {
    mote[2] = static_cast<unsigned char>(mote[2] - 1);
    if (mote[2] != 0) return;
    mote[2] = 0x10;
    mote[1] = static_cast<unsigned char>(mote[1] + 1);
}

// original 0x481E60 (EffectKind68_MoteStates[2]): the flag 0x676260 set; the
// speed +8 up 0x400 and the height +0x10 up by it; the shade down 8; the count
// down, at 0 out of use.
extern "C" void __cdecl EffectKind68_MoteRise(unsigned char* mote) {
    if (B(at::kMoteFlagB) == 0) B(at::kMoteFlagB) = 1;
    const U speed = UL(mote + 8) + 0x400u;
    SetUL(mote + 8, speed);
    SetUL(mote + 0x10, UL(mote + 0x10) + speed);
    mote[3] = static_cast<unsigned char>(mote[3] + 0xF8);
    mote[2] = static_cast<unsigned char>(mote[2] - 1);
    if (mote[2] == 0) mote[0] = 0;
}

// original 0x481EA0: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1,
// committed to slot 2); EffectGte_LoadMapCamera; the points (x - 0x20000, z,
// 0x2000000) and (.., 0x5000000) of the record's x / z projected (a, b); then
// four times: x up 0x10000; a draw mode (dtd 0) linked into the map at (x, z)
// (MapView_LinkPrimAt, 0xC); a semi-transparent POLY_F4 (a, b, then both
// heights at the new x projected) in (shade, shade, shade), linked at (x, z)
// (0x38).
extern "C" void __cdecl EffectKind68_DrawWall(unsigned shade) {
    DrawMode(1, 0x3C0, 0, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const s = S();
    alignas(4) unsigned char q[12];
    SetUL(q, UL(s + 0x34) - 0x20000u);
    SetUL(q + 4, UL(s + 0x38));
    SetUL(q + 8, 0x2000000);
    alignas(4) unsigned char a[12], b[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(a));
    SetUL(q + 8, 0x5000000);
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(b));
    const unsigned char k = static_cast<unsigned char>(shade);
    for (unsigned n = 4; n != 0; --n) {
        SetUL(q, UL(q) + 0x10000u);
        const U tp = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tp & 0xFFFFu, 0);
        SH_CALL(MapView_LinkPrimAt)(UL(q), UL(q + 4), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyF4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, a);
        Copy12(p + 0x14, b);
        SetUL(q + 8, 0x2000000);
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(a));
        SetUL(q + 8, 0x5000000);
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(b));
        Copy12(p + 0x20, a);
        Copy12(p + 0x2C, b);
        p[4] = k;
        p[5] = k;
        p[6] = k;
        SH_CALL(MapView_LinkPrimAt)(UL(q), UL(q + 4), 0, 0x38);
    }
}

void Effect3A_Inject() {
    if (bof3::WantsShadow("effect_3a")) effect_3a::SelfTest();
    BOF3_INJECT(EffectKind60_Run);
    BOF3_INJECT(EffectKind60_DrawLine);
    BOF3_INJECT(EffectKind61_Run);
    BOF3_INJECT(EffectKind61_Capture);
    BOF3_INJECT(EffectKind61_Store);
    BOF3_INJECT(EffectKind61_Scatter);
    BOF3_INJECT(EffectKind61_Arm);
    BOF3_INJECT(EffectKind61_Twinkle);
    BOF3_INJECT(EffectKind62_Run);
    BOF3_INJECT(EffectKind62_Start);
    BOF3_INJECT(EffectKind62_Rays);
    BOF3_INJECT(EffectKind62_Blast);
    BOF3_INJECT(EffectKind62_ClearRays);
    BOF3_INJECT(EffectKind62_SpawnRay);
    BOF3_INJECT(EffectKind62_StepRays);
    BOF3_INJECT(EffectKind62_DrawRay);
    BOF3_INJECT(EffectKind62_DrawRing);
    BOF3_INJECT(EffectKind64_Run);
    BOF3_INJECT(EffectKind64_Start);
    BOF3_INJECT(EffectKind64_Grow);
    BOF3_INJECT(EffectKind64_Hold);
    BOF3_INJECT(EffectKind64_Rise);
    BOF3_INJECT(EffectKind64_Burst);
    BOF3_INJECT(EffectKind64_Launch);
    BOF3_INJECT(EffectKind64_Fly);
    BOF3_INJECT(EffectKind64_FlyWait);
    BOF3_INJECT(EffectKind64_Fade);
    BOF3_INJECT(EffectKind64_DrawGlow);
    BOF3_INJECT(EffectKind64_DrawTrail);
    BOF3_INJECT(EffectKind64_ClearSparks);
    BOF3_INJECT(EffectKind64_StepSparks);
    BOF3_INJECT(EffectKind64_DrawSpark);
    BOF3_INJECT(EffectKind64_InitShard);
    BOF3_INJECT(EffectKind64_DrawShards);
    BOF3_INJECT(EffectKind68_Run);
    BOF3_INJECT(EffectKind68_Start);
    BOF3_INJECT(EffectKind68_Rise);
    BOF3_INJECT(EffectKind68_Wall);
    BOF3_INJECT(EffectKind68_End);
    BOF3_INJECT(EffectKind68_ClearMotes);
    BOF3_INJECT(EffectKind68_FindMote);
    BOF3_INJECT(EffectKind68_InitMote);
    BOF3_INJECT(EffectKind68_StepMotes);
    BOF3_INJECT(EffectKind68_DrawMote);
    BOF3_INJECT(EffectKind68_MoteGlow);
    BOF3_INJECT(EffectKind68_MoteHold);
    BOF3_INJECT(EffectKind68_MoteRise);
    BOF3_INJECT(EffectKind68_DrawWall);
}
