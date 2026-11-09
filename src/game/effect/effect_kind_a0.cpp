// Moved here by tools/move_funcs.py from effect_4e.cpp, rest_3f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_a0.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_a0_callees.h"
#include "game/effect_4e.h"
#include "game/effect_4e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3f.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_a0::at;

}  // namespace

namespace {

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

std::int32_t SL(const unsigned char* p) { return Long(p); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

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

LD I(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }

unsigned char* Bytes(long* p) { return reinterpret_cast<unsigned char*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next kind's table or data.
void DispatchEffect4e(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4e.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}

// The same, committed 0xC to `slot`.
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    SetMode(abr, x, y, dtd);
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

// The +6 tick of kind 0xA0's states 4..7: +6 up, true when it was a multiple
// of four before.
bool Tick() {
    unsigned char* const s = S();
    const unsigned char was = s[6];
    s[6] = static_cast<unsigned char>(was + 1);
    return (was & 3) == 0;
}

unsigned char* Spark(unsigned i) { return rv::Bytes(EffectKindA0_Sparks) + i * at::kSparkStride; }

unsigned char* Shard(unsigned i) { return rv::Bytes(&EffectKindA0_Shards[i]); }

// Kind 0xA0's glow: EffectKindA0_DrawGlow(+0x64, the word +0x2E, 6).
void GlowA0() {
    unsigned char* const s = S();
    SH_CALL(EffectKindA0_DrawGlow)(Point(s + 0x64), rv::As<records::EffectWork>(s)->work_2e, 6);
}

// Kind 0xA0's trail from +0x18 to +0x34.
void TrailA0(unsigned shade) {
    unsigned char* const s = S();
    SH_CALL(EffectKindA0_DrawTrail)(Point(s + 0x18), Point(s + 0x34), shade);
}

// The three swaps of kind 0xA0's states 6 and 7: +0x34..+0x3C with +0x18..+0x20.
void SwapPoints() {
    for (unsigned i = 0; i < 12; i += 4) {
        unsigned char* const s = S();
        SH_CALL(EffectKindA0_SwapLong)(Point(s + 0x34 + i), Point(s + 0x18 + i));
    }
}

}  // namespace

// The most dots EffectKindA0_DrawTrail draws before ours gives up: ordinary
// play draws about nine (the head is held at x 0x2F8000, the pull is a
// 128th of the way); the original runs on until the coordinate wraps when
// the pull's x is not negative (docs/effect_4e.md section 7).
constexpr unsigned kMostDots = 0x400;

namespace {

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

long* PointRest3f(unsigned char* p) { return reinterpret_cast<long*>(p); }

// The capture record kPixIndex names, checked against the table's three.
U PixRecord(const char* who) {
    const unsigned k = Rest3f_PixIndex;
    if (k >= at::kPixRecords)
        bof3::Fatal("%s: the capture record 0x%X is %u, past the %u of 0x%X - the original reads EffectKindA1_States' "
                    "code pointers as a rectangle (docs/rest_3f.md section 7)",
                    who, (unsigned)Rest3f_PixIndex_addr, k, at::kPixRecords, (unsigned)at::kPixTable.v);
    return ::bof3::Img(at::kPixTable.v) + 8 * k;
}

alignas(4) unsigned char g_pixels[at::kPartRoom * at::kPartStride];

U Particles() { return rest_3f::g_pixel_room ? Low32(g_pixels) : ::bof3::Img(at::kParticles.v); }

unsigned char* Particle(unsigned i) {
    if (rest_3f::g_pixel_room && i >= at::kPartRoom)
        bof3::Fatal("EffectKindA1: particle %u, past the room of %u (a capture larger than 0x60 x 0x78)", i,
                    at::kPartRoom);
    return P(Particles() + i * at::kPartStride);
}

constexpr unsigned kEffectRecords = 20;   // Effect_Objects, 20 of 0x80

// State 0 of kinds 0xA1 (0x490A80) and 0xA3 (0x491310, once the count is
// 0x29): a free effect record borrowed to draw the sprite +0x4C points at into
// VRAM (0x340, 0x100) - the rectangle cleared (0x80 x 0x100), the sprite's
// first 0x80 bytes copied into the record (rep movsd), made kind 0x3A in state
// 2, its +0x2E / +0x30 the capture record's offsets (kPixIndex = 1), its page
// set, +0x24 |= 0x88; with it current an animation set, Sprite_UpdateScreen,
// Effect_Release; Sprite_Current put back, +9 = 2, +1 up. No free record: +1
// up only. `a3` is kind 0xA3's form: the page always set and Sprite_SetAnimation(7).
void CaptureSprite(const char* who, bool a3) {
    const unsigned char k = SH_CALL(Effect_FindFree)();
    if (k == 0xFF) {
        NextState();
        return;
    }
    if (k >= kEffectRecords)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original copies a sprite past "
                    "Effect_Objects (docs/rest_3f.md section 7)",
                    who, (unsigned)k);
    unsigned char* const e = rv::Bytes(&Effect_Objects[k]);
    Rest3f_PixIndex = 1;
    SH_CALL(Gfx_ClearRect)(at::kCaptureX, at::kCaptureY, 0x80, 0x100);
    {
        const unsigned char* const from = P(rv::As<records::EffectObject>(S())->anim_data);
        for (unsigned i = 0; i < 0x80; i += 4) SetUL(e + i, UL(from + i));
    }
    e[0] = 1;
    e[1] = 2;
    e[2] = 0;
    e[5] = 0x3A;
    rv::As<records::EffectObject>(e)->screen_x = Word(P(PixRecord(who)));
    rv::As<records::EffectObject>(e)->screen_y = Word(P(PixRecord(who) + 2));
    if (a3 || (e[0x24] & 1) != 0) {
        const U tp = SH_CALL(Gpu_GetTPage)(0, 0, at::kCaptureX, at::kCaptureY);
        e[0x25] = static_cast<unsigned char>(tp);
        e[0x26] = 0x80;
    }
    e[0x24] = static_cast<unsigned char>(e[0x24] | 0x88);
    unsigned char* const old = S();
    Sprite_Current = e;
    if (a3) {
        SH_CALL(Sprite_SetAnimation)(7);
    } else if ((e[0x24] & 1) != 0) {
        if (rv::As<records::EffectObject>(e)->column == 0) {
            SH_CALL(Sprite_SetAnimationAt)(0x4D, 0x1C);
            S()[0x2A] = 0;
        } else {
            SH_CALL(Sprite_SetAnimationAt)(0x50, 0x24);
        }
    }
    SH_CALL(Sprite_UpdateScreen)();
    SH_CALL(Effect_Release)();
    Sprite_Current = old;
    old[9] = 2;
    NextState();
}

// State 1 of both (0x490BB0, 0x491410, the same code): +9 down; at 0 the
// capture record's width x height read back from VRAM (0x340, 0x100) into
// EffectKind30_Shards, the cursors put at its start and at the particles,
// the count 0, +9 = 0, +1 up.
void ReadBack(const char* who) {
    if (!CountDown()) return;
    const U rec = PixRecord(who);
    alignas(4) short rect[4] = {static_cast<short>(at::kCaptureX), static_cast<short>(at::kCaptureY),
                                static_cast<short>(Word(P(rec + 4))), static_cast<short>(Word(P(rec + 6)))};
    SH_AT(void (__cdecl*)(const short*, unsigned char*), at::kStoreImage.v)(rect, EffectKind30_Shards);
    unsigned char* const s = S();
    Rest3f_PixCursor = Low32(EffectKind30_Shards);
    Rest3f_PartCursor = Particles();   // DIV-0206: the room's start when armed
    Rest3f_PartCount = 0;
    s[9] = 0;
    NextState();
}

// State 2 of both (0x490C50, 0x4914B0, the same code): the sprite's point
// (+0x4C's +0x34..) projected; the rows +9 * h / 2 .. (+9 + 1) * h / 2 of the
// read-back walked by the pixel cursor (w a row), every pixel not 0 a
// particle at the cursor: its pixel, x = screen x - dx + column, y = screen y -
// dy + row, the depth copied (fld / fstp), the count up. +9 up; at 2, +1 up.
void SplitPixels(const char* who) {
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    {
        alignas(4) long q[3];
        unsigned char* const sprite = P(rv::As<records::EffectObject>(S())->anim_data);
        q[0] = Long(sprite + 0x34);
        q[1] = Long(sprite + 0x38);
        q[2] = Long(sprite + 0x3C);
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o));
    }
    const U rec = PixRecord(who);
    unsigned char* const s = S();
    const U h = Word(P(rec + 6));
    // imul dl: the low byte of (h >> 1) times +9 (the low bytes of the signed
    // and the unsigned products agree)
    unsigned row = static_cast<unsigned char>(((h >> 1) & 0xFFu) * s[9]);
    for (;;) {
        const std::int32_t bound = (SW(P(rec + 6)) >> 1) * (static_cast<std::int32_t>(s[9]) + 1);
        if (static_cast<std::int32_t>(row) >= bound) break;
        if (bound > 0xFF)
            bof3::Fatal("%s: +9 is %u, so the rows run to %d - the original's row is a byte and never reaches it "
                        "(docs/rest_3f.md section 7)",
                        who, (unsigned)s[9], (int)bound);
        if (SW(P(rec + 4)) > 0) {
            unsigned column = 0;
            do {
                unsigned char* const pixel = P(Rest3f_PixCursor);
                const U v = Word(pixel);
                if (v != 0) {
                    if (rest_3f::g_pixel_room && Rest3f_PartCount >= at::kPartRoom)
                        bof3::Fatal("%s: particle %u, past the room of %u (DIV-0206)", who, (unsigned)Rest3f_PartCount,
                                    at::kPartRoom);
                    SetWord(P(Rest3f_PartCursor) + 2, v);
                    StF(P(Rest3f_PartCursor) + 4, F(o) - I(static_cast<U>(SW(P(rec)))) + I(column));
                    StF(P(Rest3f_PartCursor) + 8, F(o + 4) - I(static_cast<U>(SW(P(rec + 2)))) + I(row));
                    StF(P(Rest3f_PartCursor) + 0xC, F(o + 8));
                    Rest3f_PartCount = Rest3f_PartCount + 1u;
                    Rest3f_PartCursor = Rest3f_PartCursor + at::kPartStride;
                }
                Rest3f_PixCursor = Rest3f_PixCursor + 2;
                column = (column + 1) & 0xFFu;
            } while (static_cast<std::int32_t>(column) < SW(P(rec + 4)));
        }
        row = (row + 1) & 0xFFu;
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    unsigned char* const t = S();
    if (t[9] == 2) {
        // DIV-0206: the count on every capture, so a sprite that overran the
        // original's room (1,306 before Sprite_Current) is in the log.
        bof3::Log("kind 0x%02X: capture %d x %d, %u particles%s", (unsigned)t[5], (int)SW(P(rec + 4)),
                  (int)SW(P(rec + 6)), (unsigned)Rest3f_PartCount, Rest3f_PartCount > 1306 ? " (past the original's room)" : "");
        t[1] = static_cast<unsigned char>(t[1] + 1);
    }
}

// The shuffle both aims share: every sixteenth particle, sixteen swaps of two
// entries of `order` by Rand & 0xF (the first then the second).
void Shuffle(unsigned char* order) {
    for (unsigned n = 0; n < 16; ++n) {
        const unsigned a = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
        const unsigned b = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
        const unsigned char x = order[a];
        const unsigned char y = order[b];
        order[b] = x;
        order[a] = y;
    }
}

// Two steps of 0..0xF, the larger first, the second negated.
void Steps(unsigned char* part) {
    part[0x10] = static_cast<unsigned char>(SH_CALL(Rand)() & 0xF);
    part[0x11] = static_cast<unsigned char>(SH_CALL(Rand)() & 0xF);
    if (static_cast<signed char>(part[0x10]) < static_cast<signed char>(part[0x11])) {
        const unsigned char t = part[0x10];
        part[0x10] = part[0x11];
        part[0x11] = t;
    }
    part[0x11] = static_cast<unsigned char>(0u - part[0x11]);
}

// The particle d steps back (`sign` -1) or on (+1) along its step: x, y each
// minus / plus step * d (fild; fsubr / fadd).
void Offset(unsigned char* part, unsigned d, int sign) {
    const U ex = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(part[0x10])) * static_cast<std::int32_t>(d));
    const U ey = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(part[0x11])) * static_cast<std::int32_t>(d));
    if (sign < 0) {
        StF(part + 4, F(part + 4) - I(ex));
        StF(part + 8, F(part + 8) - I(ey));
    } else {
        StF(part + 4, I(ex) + F(part + 4));
        StF(part + 8, I(ey) + F(part + 8));
    }
}

// The end both aims share: +9 = 0x50, +1 up, sound 0x20C.
void AimEnd() {
    S()[9] = 0x50;
    NextState();
    SH_CALL(Sound_PlayEffect)(at::kSoundPixels);
}

// A particle drawn: a TILE_1 at its x, y, depth, coloured from its 15-bit
// pixel (r = low byte << 3, g = bits 5..9, b = bits 10..14, each << 3),
// committed 0x14 to slot 2.
void DrawPixel(const unsigned char* part) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SetUL(p + 8, UL(part + 4));
    SetUL(p + 0xC, UL(part + 8));
    SetUL(p + 0x10, UL(part + 0xC));
    p[4] = static_cast<unsigned char>(part[2] << 3);
    p[5] = static_cast<unsigned char>((Word(part + 2) >> 2) & 0xF8u);
    p[6] = static_cast<unsigned char>((Word(part + 2) >> 7) & 0xF8u);
    SH_CALL(Gfx_CommitPrim)(2, 0x14);
}

}  // namespace

// original 0x48FC20 (hidden in 0x48FA80): jmp [EffectKindA0_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA0_Run(void) {
    DispatchEffect4e("EffectKindA0_Run", EffectKindA0_States, EffectKindA0_States_count);
}

// original 0x48FC40 (state 0): the glow's point +0x64..+0x6C the leader's
// (ObjTrio +0x34..), its height 0x800000 more; the size +0x2E = 0; +9 = 8; +1
// up; Sound_PlayEffect(0x20B).
extern "C" void __cdecl EffectKindA0_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[0], static_cast<std::uint32_t>(ObjTrio[0].x));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[1], static_cast<std::uint32_t>(ObjTrio[0].z));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[2], static_cast<std::uint32_t>(ObjTrio[0].height) + 0x800000u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    S()[9] = 8;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x20B);
}

// original 0x48FCA0 (state 1): the size +0x2E up 0x20, the glow; +9 down, at
// 0: +9 = 0x40, +1 up.
extern "C" void __cdecl EffectKindA0_Grow(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e + 0x20u);
    GlowA0();
    if (!CountDown()) return;
    S()[9] = 0x40;
    NextState();
}

// original 0x48FCF0 (state 2): the glow; +9 down, at 0: +9 = 0x20, +1 up.
extern "C" void __cdecl EffectKindA0_Hold(void) {
    GlowA0();
    if (!CountDown()) return;
    S()[9] = 0x20;
    NextState();
}

// original 0x48FD30 (state 3): the glow's height +0x6C up 0xC0000, its size
// down 6, the glow; +9 down, at 0: +9 = 0x20, +6 = 0, +1 up, the sparks
// cleared, the point +0x34..+0x3C = +0x64..+0x6C, the first sixteen shards
// set up there (EffectKindA0_InitShard), the second sixteen's speed and shade
// words (+0x28, +0x2A) 0.
extern "C" void __cdecl EffectKindA0_Rise(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->angle[2], rv::As<records::EffectObject>(s)->angle[2] + 0xC0000u);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e - 6u);
    GlowA0();
    if (!CountDown()) return;
    S()[9] = 0x20;
    S()[6] = 0;
    NextState();
    SH_CALL(EffectKindA0_ClearSparks)();
    rv::Current<records::EffectObject>()->x = rv::As<records::EffectObject>(S())->angle[0];
    rv::Current<records::EffectObject>()->z = rv::As<records::EffectObject>(S())->angle[1];
    rv::Current<records::EffectObject>()->height = rv::As<records::EffectObject>(S())->angle[2];
    for (unsigned i = 0; i < at::kShardRun; ++i) SH_CALL(EffectKindA0_InitShard)(Shard(i));
    for (unsigned i = at::kShardRun; i < at::kShardCount; ++i) {
        SetWord(Shard(i) + 0x28, 0);
        SetWord(Shard(i) + 0x2A, 0);
    }
}

// original 0x48FDF0 (state 4): the +6 tick (a spark spawned every fourth),
// the shards drawn, the sparks stepped; once the chapter's count 0x903848 is
// 0x14: the trail's head +0x18..+0x20 = the glow's point, its speed +0xC..
// +0x14 = 0, its pull +0x64..+0x6C = ((0x2F8000, 0x278000, 0xF000000) - the
// point) sar 7; +1 up.
extern "C" void __cdecl EffectKindA0_Sparkle(void) {
    if (Tick()) SH_CALL(EffectKindA0_SpawnSpark)();
    SH_CALL(EffectKindA0_DrawShards)();
    SH_CALL(EffectKindA0_StepSparks)();
    if (At(::bof3::Img(at::kCounter.v))[0] != 0x14) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, rv::As<records::EffectObject>(S())->angle[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, rv::As<records::EffectObject>(S())->angle[1]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, rv::As<records::EffectObject>(S())->angle[2]);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->angle[0], Sar(0x2F8000u - rv::As<records::EffectObject>(s)->angle[0], 7));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->angle[1], Sar(0x278000u - rv::As<records::EffectObject>(s)->angle[1], 7));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->angle[2], Sar(0xF000000u - rv::As<records::EffectObject>(s)->angle[2], 7));
    NextState();
}

// original 0x48FEB0 (state 5): the tick, the shards, the sparks; +9 = 0x20,
// +1 up, Sound_PlayEffect(0x209).
extern "C" void __cdecl EffectKindA0_Launch(void) {
    if (Tick()) SH_CALL(EffectKindA0_SpawnSpark)();
    SH_CALL(EffectKindA0_DrawShards)();
    SH_CALL(EffectKindA0_StepSparks)();
    S()[9] = 0x20;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x48FF00 (state 6): the tick, the sparks, the shards, the trail
// (shade 0x40); the speed +0xC.. up by the pull +0x64.., the head +0x18.. up
// by the speed, held at (0x2F8000, 0x278000, 0xF000000) once its x is below
// 0x2F8000; the shade +3 down 4; +9 down, at 0: the second sixteen shards
// set up at the head (the points swapped round EffectKindA0_InitShard), +9 =
// 0x80, +1 up.
extern "C" void __cdecl EffectKindA0_Fly(void) {
    if (Tick()) SH_CALL(EffectKindA0_SpawnSpark)();
    SH_CALL(EffectKindA0_StepSparks)();
    SH_CALL(EffectKindA0_DrawShards)();
    TrailA0(0x40);
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + rv::As<records::EffectObject>(s)->angle[0]);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, rv::As<records::EffectWork>(s)->work_10 + rv::As<records::EffectObject>(s)->angle[1]);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y) + rv::As<records::EffectObject>(s)->angle[2]);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + rv::As<records::EffectWork>(s)->work_0c);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + rv::As<records::EffectWork>(s)->work_10);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y));
    s = S();
    if (SL(s + 0x18) < 0x2F8000) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18, 0x2F8000);
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, 0x278000);
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, 0xF000000);
        s = S();
    }
    s[3] = static_cast<unsigned char>(s[3] + 0xFC);
    if (!CountDown()) return;
    SwapPoints();
    for (unsigned i = at::kShardRun; i < at::kShardCount; ++i) SH_CALL(EffectKindA0_InitShard)(Shard(i));
    SwapPoints();
    S()[9] = 0x80;
    NextState();
}

// original 0x490090 (state 7): the tick - every fourth a spark at the point
// and one at the head (the points swapped round the second spawn); the
// sparks, the shards, the trail (0x40); once the chapter's count is 0x17:
// the shade +3 = 0x80, +9 = 8, +1 up, Sound_PlayEffect(0x20A).
extern "C" void __cdecl EffectKindA0_FlyWait(void) {
    if (Tick()) {
        SH_CALL(EffectKindA0_SpawnSpark)();
        SwapPoints();
        SH_CALL(EffectKindA0_SpawnSpark)();
        SwapPoints();
    }
    SH_CALL(EffectKindA0_StepSparks)();
    SH_CALL(EffectKindA0_DrawShards)();
    TrailA0(0x40);
    if (At(::bof3::Img(at::kCounter.v))[0] != 0x17) return;
    S()[3] = 0x80;
    S()[9] = 8;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x20A);
}

// original 0x490180 (state 8): the size +0x2E down 0x10, the shade +3 down 8;
// the sparks; the trail in that shade; +9 down, at 0 Effect_Release (a tail
// jmp).
extern "C" void __cdecl EffectKindA0_Fade(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e - 0x10u);
    s = S();
    s[3] = static_cast<unsigned char>(s[3] + 0xF8);
    SH_CALL(EffectKindA0_StepSparks)();
    TrailA0(S()[3]);
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x4901D0: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), dtd 1)
// committed to slot 2; EffectGte_LoadMapCamera; o = `point` projected; r =
// EffectGte_ProjectSize(point, {size, size})'s first word + (Frame_Counter &
// 1), a word; a fan of 32 semi-transparent POLY_G3 round o - the centre in
// the colour bits' shades ((c & 0xFC) << 5, (c & 0xFE) << 6, c << 7, each a
// byte), the rim black at o + (cos, sin) * r sar 12 from the angle 0x80 by
// 0x80 (the first rim point o.x + r) - every vertex at o's depth; each
// committed 0x34 to slot 2. EffectKind64_DrawGlow's form, without its
// unwritten rim depth (DIV-0068's site is that function, not this one).
extern "C" void __cdecl EffectKindA0_DrawGlow(const long* point, unsigned size, unsigned colour) {
    DrawMode(1, 0x2C0, 0x100, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(point, Out(o));
    alignas(4) short sz[2] = {static_cast<short>(size), static_cast<short>(size)};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(point, sz, r);
    const std::int32_t radius = static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u));
    const unsigned char c = static_cast<unsigned char>(colour);
    const unsigned char red = static_cast<unsigned char>((c & 0xFC) << 5);
    const unsigned char green = static_cast<unsigned char>((c & 0xFE) << 6);
    const unsigned char blue = static_cast<unsigned char>(c << 7);
    alignas(4) unsigned char rim[8];
    StF(rim, I(static_cast<U>(radius)) + F(o));
    SetUL(rim + 4, UL(o + 4));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        angle += 0x80;
        const int t = static_cast<int>(angle & 0xFFFFu);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        SetUL(p + 0x18, UL(rim));
        SetUL(p + 0x1C, UL(rim + 4));
        const U dx = MulSar(SH_CALL(Math_Cos)(t), static_cast<U>(radius), 12);
        StF(rim, I(dx) + F(o));
        const U dy = MulSar(SH_CALL(Math_Sin)(t), static_cast<U>(radius), 12);
        SetUL(p + 0x28, UL(rim));
        StF(rim + 4, I(dy) + F(o + 4));
        SetUL(p + 0x2C, UL(rim + 4));
        SetUL(p + 0x30, UL(o + 8));
        SetUL(p + 0x20, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        p[4] = red;
        p[5] = green;
        p[6] = blue;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[0x25] = 0;
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x34);
    }
}

// original 0x490390: two semi-transparent POLY_G4 along the trail from `from`
// projected (a) to `to` projected (b) - the first (a + (h, -h), a, b + (h,
// -h), b) with a and b shaded (shade, shade, 0), the second (a, a + (-h, h),
// b, b + (-h, h)) with the offset corners black and a, b shaded, h the float at
// 0x5C41B8 - each committed 0x44 to slot 2 (EffectKind64_DrawTrail's two
// quads); then TILE_1 dots (shade, shade, 0) from q = `to` + (Frame_Counter &
// 0xF) * the pull (+0x64..+0x6C, Sprite_Current read afresh), each step the
// pull << 4, while q.x is above from's (signed) - no count, unlike kind
// 0x64's eight.
extern "C" void __cdecl EffectKindA0_DrawTrail(const long* from, const long* to, unsigned shade) {
    const unsigned char* const a = Bytes(from);
    const unsigned char* const b = Bytes(to);
    const LD half = F(At(at::kHalf));
    const unsigned char k = static_cast<unsigned char>(shade);
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
    const unsigned char* s = S();
    const U f = Frame_Counter & 0xFu;
    alignas(4) unsigned char q[12];
    SetUL(q, f * rv::As<records::EffectObject>(s)->angle[0] + UL(b));
    SetUL(q + 4, f * rv::As<records::EffectObject>(s)->angle[1] + UL(b + 4));
    SetUL(q + 8, f * rv::As<records::EffectObject>(s)->angle[2] + UL(b + 8));
    for (unsigned dots = 0; SL(q) > SL(a); ++dots) {
        if (dots == kMostDots)
            bof3::Fatal("EffectKindA0_DrawTrail: %u dots and x 0x%X still above 0x%X (pull x 0x%X) - the original "
                        "draws on until the coordinate wraps (docs/effect_4e.md section 7)",
                        dots, (unsigned)UL(q), (unsigned)UL(a), (unsigned)rv::As<records::EffectObject>(S())->angle[0]);
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o));
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o);
        p[4] = k;
        p[5] = k;
        p[6] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x14);
        s = S();
        SetUL(q, UL(q) + (rv::As<records::EffectObject>(s)->angle[0] << 4));
        SetUL(q + 4, UL(q + 4) + (rv::As<records::EffectObject>(s)->angle[1] << 4));
        SetUL(q + 8, UL(q + 8) + (rv::As<records::EffectObject>(s)->angle[2] << 4));
    }
}

// original 0x490630: the sixteen sparks of EffectKindA0_Sparks out of use.
extern "C" void __cdecl EffectKindA0_ClearSparks(void) {
    for (unsigned i = 0; i < at::kSparkCount; ++i) Spark(i)[0] = 0;
}

// original 0x490650: the first spark out of use (none: nothing) put in use at
// Sprite_Current's point +0x34..: +1 = 0, life +2 = 0x10, size +0x14 = 0x100,
// shades +0x16 = 0x60, +0x17 = 0.
extern "C" void __cdecl EffectKindA0_SpawnSpark(void) {
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] != 0) continue;
        const unsigned char* const s = S();
        auto* const so = rv::AsConst<records::EffectObject>(s);
        k[0] = 1;
        k[1] = 0;
        k[2] = 0x10;
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->x, static_cast<std::uint32_t>(so->x));
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->z, static_cast<std::uint32_t>(so->z));
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->height, static_cast<std::uint32_t>(so->height));
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->size, 0x100);
        k[0x16] = 0x60;
        k[0x17] = 0;
        return;
    }
}

// original 0x4906A0: EffectGte_LoadMapCamera; each spark in use: its size
// +0x14 down 0x10, its life +2 down (at 0 out of use), drawn
// (EffectKind64_DrawSpark, group E3A's). Answers 1 when a spark was in use,
// else 0 (EffectKind64_StepSparks' form over the sixteen).
extern "C" unsigned char __cdecl EffectKindA0_StepSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] == 0) continue;
        const unsigned char life = k[2];
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->size, rv::As<records::EffectGlowSpark>(k)->size + 0xFFF0u);
        any = 1;
        k[2] = static_cast<unsigned char>(life - 1);
        if (k[2] == 0) k[0] = 0;
        SH_CALL(EffectKind64_DrawSpark)(k);
    }
    return any;
}

// original 0x4906F0: the shard's point +0..+8 the record's (+0x34..); three
// Rand angles x = & 0xFFF, y = (& 0x7FF) - 0x400, z = & 0xFFF; its two edge
// vectors +0x10 = (cos 0x10, sin 0x10, 0) and +0x18 = (cos -0x10, sin -0x10,
// 0) as words, turned by the matrix EffectGte_SetDiagonalOne, Gte_RotMatrixX
// (x), Gte_RotMatrixY (-y), Gte_RotMatrixZ (z) make (0x5A7C70, in place); its
// shade +0x2A = 0x40, its speed +0x28 = (Rand & 2) + 3, its angles
// +0x20..+0x25 = 0 (EffectKind64_InitShard's form; that one's shade is 0x20).
extern "C" void __cdecl EffectKindA0_InitShard(unsigned char* shard) {
    SetUL(shard, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->z, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->height, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
    const U x = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U y = (static_cast<U>(SH_CALL(Rand)()) & 0x7FFu) - 0x400u;
    const U z = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->edge0.x, static_cast<U>(SH_CALL(Math_Cos)(0x10)));
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->edge0.y, static_cast<U>(SH_CALL(Math_Sin)(0x10)));
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->edge0.z, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->edge1.x, static_cast<U>(SH_CALL(Math_Cos)(-0x10)));
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->edge1.y, static_cast<U>(SH_CALL(Math_Sin)(-0x10)));
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->edge1.z, 0);
    alignas(4) short m[16];
    SH_CALL(EffectGte_SetDiagonalOne)(m);
    SH_CALL(Gte_RotMatrixX)(S16(x), m);
    SH_CALL(Gte_RotMatrixY)(-S16(y), m);
    SH_CALL(Gte_RotMatrixZ)(S16(z), m);
    using Turn = void (__cdecl*)(const short*, short*, short*);
    SH_AT(Turn, at::kMatrixVector.v)(m, reinterpret_cast<short*>(shard + 0x10), reinterpret_cast<short*>(shard + 0x10));
    SH_AT(Turn, at::kMatrixVector.v)(m, reinterpret_cast<short*>(shard + 0x18), reinterpret_cast<short*>(shard + 0x18));
    const U speed = (static_cast<U>(SH_CALL(Rand)()) & 2u) + 3u;
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->shade, 0x40);
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->speed, speed);
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->angle[0], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->angle[1], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->angle[2], 0);
}

// original 0x490810: a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1)
// committed to slot 2; EffectGte_LoadMapCamera; each of the 32 shards of
// EffectKindA0_Shards drawn (EffectKindA0_DrawShard) and its angle +0x24 up
// 0x10.
extern "C" void __cdecl EffectKindA0_DrawShards(void) {
    DrawMode(1, 0x380, 0x100, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kShardCount; ++i) {
        unsigned char* const shard = Shard(i);
        SH_CALL(EffectKindA0_DrawShard)(shard);
        rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->angle[2], rv::As<records::EffectShard>(shard)->angle[2] + 0x10u);
    }
}

// original 0x490880: a semi-transparent POLY_G3: the shard's point projected,
// and the point plus each edge vector (+0x10, +0x18: three s16) turned by
// the angle +0x24 in its first two components - ((cos e0 - sin e1) sar 8,
// (sin e0 + cos e1) sar 8) - each times the speed +0x28, the third << 12
// times the speed; the shade +0x2A held to 0..0xFF on the first two
// channels of the first vertex, the rest black; committed 0x34 to slot 2.
extern "C" void __cdecl EffectKindA0_DrawShard(unsigned char* shard) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(Point(shard), Out(p + 8));
    alignas(4) unsigned char v[12];
    for (unsigned e = 0; e < 2; ++e) {
        const unsigned char* const edge = shard + 0x10 + 8 * e;
        int c = SH_CALL(Math_Cos)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2])));
        U x = static_cast<U>(c) * static_cast<U>(SW(edge));
        int s = SH_CALL(Math_Sin)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2])));
        x = Sar(x - static_cast<U>(s) * static_cast<U>(SW(edge + 2)), 8) * static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->speed)));
        s = SH_CALL(Math_Sin)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2])));
        U y = static_cast<U>(s) * static_cast<U>(SW(edge));
        c = SH_CALL(Math_Cos)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2])));
        y += static_cast<U>(c) * static_cast<U>(SW(edge + 2));
        const U speed = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->speed)));
        const U z = (static_cast<U>(SW(edge + 4)) << 12) * speed + static_cast<std::uint32_t>(rv::As<records::EffectShard>(shard)->height);
        y = Sar(y, 8) * speed + static_cast<std::uint32_t>(rv::As<records::EffectShard>(shard)->z);
        SetUL(v, x + UL(shard));
        SetUL(v + 4, y);
        SetUL(v + 8, z);
        SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x18 + 0x10 * e));
    }
    const std::int32_t w = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->shade));
    const unsigned char k = w < 0 ? 0 : w > 0xFF ? 0xFF : shard[0x2A];
    p[4] = k;
    p[5] = k;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x34);
}

// original 0x490A30: the dwords at `a` and `b` swapped by three xors (a and b
// the same: both 0, as the xors make it).
extern "C" void __cdecl EffectKindA0_SwapLong(long* a, long* b) {
    unsigned char* const pa = Bytes(a);
    unsigned char* const pb = Bytes(b);
    SetUL(pa, UL(pa) ^ UL(pb));
    SetUL(pb, UL(pb) ^ UL(pa));
    SetUL(pa, UL(pa) ^ UL(pb));
}

// original 0x490A60 (hidden in 0x490A30): jmp [EffectKindA1_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA1_Run(void) {
    DispatchEffect4e("EffectKindA1_Run", EffectKindA1_States, EffectKindA1_States_count);
}

// original 0x490A80 (state 0, hidden in E4E's EffectKindA0_SwapLong; PSX twin
// 0x801D21B4): CaptureSprite, the page set only when the copy's +0x24 bit 0
// is, and the animation (0x4D from 0x1C, +0x2A 0, when its word +0x2C is 0;
// else 0x50 from 0x24) only then.
extern "C" void __cdecl EffectKindA1_CaptureSprite(void) { CaptureSprite("EffectKindA1_CaptureSprite", false); }

// original 0x490BB0 (state 1, hidden in EffectKindA0_SwapLong): ReadBack.
extern "C" void __cdecl EffectKindA1_ReadBack(void) { ReadBack("EffectKindA1_ReadBack"); }

// original 0x490C50 (state 2, hidden in EffectKindA0_SwapLong): SplitPixels.
extern "C" void __cdecl EffectKindA1_SplitPixels(void) { SplitPixels("EffectKindA1_SplitPixels"); }

// original 0x490E20 (state 3, hidden in EffectKindA0_SwapLong): every particle
// its blink (an order of eight 0s and eight 1s shuffled every sixteenth) and
// its step - +0xB 0: (Rand & 0x1F) - 0x10 each; else Steps - and, +6 0: a
// delay d = (Rand & 0x1E) + 0xA and the particle d steps back; else the delay
// 0x28 - (Rand & 0x1E). AimEnd.
extern "C" void __cdecl EffectKindA1_AimPixels(void) {
    unsigned char order[16];
    for (unsigned n = 0; n < 16; ++n) order[n] = static_cast<unsigned char>(n & 1);
    unsigned j = 0;
    for (U i = 0; (i & 0xFFFFu) < Rest3f_PartCount; ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if ((i & 0xF) == 0) {
            Shuffle(order);
            j = 0;
        }
        const unsigned char* s = S();
        part[0] = order[j & 0xFFFF];
        ++j;
        if (s[0xB] == 0) {
            part[0x10] = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1F) - 0x10);
            part[0x11] = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1F) - 0x10);
        } else {
            Steps(part);
        }
        s = S();
        if (s[6] == 0) {
            const unsigned d = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1E) + 0xA);
            part[1] = static_cast<unsigned char>(d);
            Offset(part, d, -1);
        } else {
            part[1] = static_cast<unsigned char>(0x28 - (SH_CALL(Rand)() & 0x1E));
        }
    }
    AimEnd();
}

// original 0x490FA0 (state 4, hidden in EffectKindA0_SwapLong): the sprite's
// byte +0 |= 0x40; every particle shown this frame (+0 not 0): +6 0 - while its
// delay runs it steps (and the delay counts down); else it waits out its delay
// and then steps; drawn (DrawPixel). Every particle's +0 flipped. +9 down, at
// 0 +1 up.
extern "C" void __cdecl EffectKindA1_MovePixels(void) {
    unsigned char* const sprite = P(rv::As<records::EffectObject>(S())->anim_data);
    sprite[0] = static_cast<unsigned char>(sprite[0] | 0x40);
    for (U i = 0; (i & 0xFFFFu) < Rest3f_PartCount; ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if (part[0] != 0) {
            if (S()[6] == 0) {
                if (part[1] != 0) {
                    part[1] = static_cast<unsigned char>(part[1] - 1);
                    Offset(part, 1, 1);
                }
            } else if (part[1] != 0) {
                part[1] = static_cast<unsigned char>(part[1] - 1);
            } else {
                Offset(part, 1, 1);
            }
            DrawPixel(part);
        }
        part[0] = static_cast<unsigned char>(part[0] ^ 1);
    }
    if (CountDown()) NextState();
}

// original 0x4910D0 (hidden in 0x490A30): jmp [EffectKindA2_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA2_Run(void) {
    DispatchEffect4e("EffectKindA2_Run", EffectKindA2_States, EffectKindA2_States_count);
}

// original 0x4910F0 (state 0, hidden in EffectKindA0_SwapLong): the radii
// +0xC, +0x10 and their speeds +0x18, +0x1C 0; +9 = 0x14, +1 up; sound 0x208.
extern "C" void __cdecl EffectKindA2_Start(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_0c = 0;
    rv::As<records::EffectWork>(s)->work_10 = 0;
    rv::As<records::EffectObject>(s)->work_18 = 0;
    rv::As<records::EffectObject>(s)->work_1c = 0;
    s[9] = 0x14;
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(at::kSoundRing);
}

// original 0x491140 (state 1, hidden in EffectKindA0_SwapLong): the speeds up
// 0x200 / 0x100, the radii up by them; the ring (+0x34, +0xC, +0x10); +9 down,
// at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA2_Grow(void) {
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->work_18 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + 0x200u;
    rv::As<records::EffectObject>(s)->work_1c = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x100u;
    rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18);
    rv::As<records::EffectWork>(s)->work_10 = rv::As<records::EffectWork>(s)->work_10 + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c);
    SH_CALL(EffectKindA2_DrawRing)(PointRest3f(s + 0x34), static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c), static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10));
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x4911C0 (state 2, hidden in EffectKindA0_SwapLong): the radii and
// speeds 0, then twenty steps of the speeds (+0x100 / +0x200) and the radii by
// them, the radii negated; +9 = 0x14, +1 up; sound 0x208.
extern "C" void __cdecl EffectKindA2_Rewind(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_0c = 0;
    rv::As<records::EffectWork>(s)->work_10 = 0;
    rv::As<records::EffectObject>(s)->work_18 = 0;
    rv::As<records::EffectObject>(s)->work_1c = 0;
    for (unsigned n = 0; n < 0x14; ++n) {
        rv::As<records::EffectObject>(s)->work_18 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + 0x100u;
        rv::As<records::EffectObject>(s)->work_1c = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x200u;
        rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18);
        rv::As<records::EffectWork>(s)->work_10 = rv::As<records::EffectWork>(s)->work_10 + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c);
    }
    rv::As<records::EffectWork>(s)->work_0c = 0u - rv::As<records::EffectWork>(s)->work_0c;
    rv::As<records::EffectWork>(s)->work_10 = 0u - rv::As<records::EffectWork>(s)->work_10;
    s[9] = 0x14;
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(at::kSoundRing);
}

// original 0x491270 (state 3, hidden in EffectKindA0_SwapLong): the ring; the
// radii up by the speeds, the speeds down 0x100 / 0x200; +9 down, at 0
// Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA2_Shrink(void) {
    unsigned char* s = S();
    SH_CALL(EffectKindA2_DrawRing)(PointRest3f(s + 0x34), static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c), static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10));
    s = S();
    rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18);
    rv::As<records::EffectWork>(s)->work_10 = rv::As<records::EffectWork>(s)->work_10 + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c);
    rv::As<records::EffectObject>(s)->work_18 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + 0xFFFFFF00u;
    rv::As<records::EffectObject>(s)->work_1c = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0xFFFFFE00u;
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x4912F0 (in no list; between catalog part 6 rows): jmp
// [EffectKindA3_States + +1 * 4], unbounded. Its first five states are kind
// 0xA1's.
extern "C" void __cdecl EffectKindA3_Run(void) {
    DispatchEffect4e("EffectKindA3_Run", EffectKindA3_States, EffectKindA3_States_count);
}

// original 0x491310 (kind 0xA3's state 5, hidden in EffectKindA0_SwapLong):
// nothing until the chapter's count is 0x29; then CaptureSprite in kind
// 0xA3's form (the page always, Sprite_SetAnimation(7)).
extern "C" void __cdecl EffectKindA3_CaptureSprite(void) {
    if (At(::bof3::Img(at::kCounter.v))[0] != 0x29) return;
    CaptureSprite("EffectKindA3_CaptureSprite", true);
}

// original 0x491410 (state 6, hidden in EffectKindA0_SwapLong): ReadBack (the
// same code as 0x490BB0).
extern "C" void __cdecl EffectKindA3_ReadBack(void) { ReadBack("EffectKindA3_ReadBack"); }

// original 0x4914B0 (state 7, hidden in EffectKindA0_SwapLong): SplitPixels
// (the same code as 0x490C50).
extern "C" void __cdecl EffectKindA3_SplitPixels(void) { SplitPixels("EffectKindA3_SplitPixels"); }

// original 0x491680 (state 8, hidden in EffectKindA0_SwapLong): every particle
// its blink (the shuffled order) and Steps, a delay d = (Rand & 0x1E) + 0xA and
// the particle d steps on (it comes back in state 9). AimEnd.
extern "C" void __cdecl EffectKindA3_AimPixels(void) {
    unsigned char order[16];
    for (unsigned n = 0; n < 16; ++n) order[n] = static_cast<unsigned char>(n & 1);
    unsigned j = 0;
    for (U i = 0; (i & 0xFFFFu) < Rest3f_PartCount; ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if ((i & 0xF) == 0) {
            Shuffle(order);
            j = 0;
        }
        part[0] = order[j & 0xFFFF];
        ++j;
        Steps(part);
        const unsigned d = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1E) + 0xA);
        part[1] = static_cast<unsigned char>(d);
        Offset(part, d, 1);
    }
    AimEnd();
}

// original 0x4917C0 (state 9, hidden in EffectKindA0_SwapLong): the sprite's
// byte +0 |= 0x40; every particle shown this frame steps back while its delay
// runs and is drawn; every +0 flipped. +9 down, at 0 the chapter's count =
// 0x2B and +1 up.
extern "C" void __cdecl EffectKindA3_MovePixels(void) {
    unsigned char* const sprite = P(rv::As<records::EffectObject>(S())->anim_data);
    sprite[0] = static_cast<unsigned char>(sprite[0] | 0x40);
    for (U i = 0; (i & 0xFFFFu) < Rest3f_PartCount; ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if (part[0] != 0) {
            if (part[1] != 0) {
                part[1] = static_cast<unsigned char>(part[1] - 1);
                Offset(part, 1, -1);
            }
            DrawPixel(part);
        }
        part[0] = static_cast<unsigned char>(part[0] ^ 1);
    }
    if (CountDown()) {
        At(::bof3::Img(at::kCounter.v))[0] = 0x2B;
        NextState();
    }
}

// original 0x4918B0 (point, inner, outer): a draw mode (Gpu_GetTPage(0, 1,
// 0x2C0, 0x100), dtd 0) committed 0xC to slot 2; EffectGte_LoadMapCamera; a
// ring of 32 semi-transparent POLY_G4 round the point in x and y - inner corners
// at the point + ((cos, sin) * inner sar 8), outer at outer's, the angle 0x80
// by 0x80 (the first edge the point + (inner << 4, 0) and (outer << 4, 0)),
// each corner projected (the point's z); inner corners (0xFF, 0x7F, 0), outer
// black; each committed 0x44 to slot 2. al 0.
extern "C" unsigned char __cdecl EffectKindA2_DrawRing(const long* point, long inner, long outer) {
    SetMode(1, 0x2C0, 0x100, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) long q[3];
    alignas(4) unsigned char o0[12], o1[12];
    q[0] = static_cast<long>((static_cast<U>(inner) << 4) + static_cast<U>(point[0]));
    q[1] = point[1];
    q[2] = point[2];
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o0));
    q[0] = static_cast<long>((static_cast<U>(outer) << 4) + static_cast<U>(point[0]));
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o1));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        angle += 0x80;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o0);
        Copy12(p + 0x18, o1);
        const int t = static_cast<int>(angle & 0xFFFFu);
        const U c1 = static_cast<U>(SH_CALL(Math_Cos)(t));
        q[0] = static_cast<long>(Sar(c1 * static_cast<U>(inner), 8) + static_cast<U>(point[0]));
        const U s1 = static_cast<U>(SH_CALL(Math_Sin)(t));
        q[1] = static_cast<long>(Sar(s1 * static_cast<U>(inner), 8) + static_cast<U>(point[1]));
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o0));
        const U c2 = static_cast<U>(SH_CALL(Math_Cos)(t));
        q[0] = static_cast<long>(Sar(c2 * static_cast<U>(outer), 8) + static_cast<U>(point[0]));
        const U s2 = static_cast<U>(SH_CALL(Math_Sin)(t));
        q[1] = static_cast<long>(Sar(s2 * static_cast<U>(outer), 8) + static_cast<U>(point[1]));
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o1));
        Copy12(p + 0x28, o0);
        Copy12(p + 0x38, o1);
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

void EffectKindA0_Inject() {
    BOF3_INJECT(EffectKindA0_Run);
    BOF3_INJECT(EffectKindA0_Start);
    BOF3_INJECT(EffectKindA0_Grow);
    BOF3_INJECT(EffectKindA0_Hold);
    BOF3_INJECT(EffectKindA0_Rise);
    BOF3_INJECT(EffectKindA0_Sparkle);
    BOF3_INJECT(EffectKindA0_Launch);
    BOF3_INJECT(EffectKindA0_Fly);
    BOF3_INJECT(EffectKindA0_FlyWait);
    BOF3_INJECT(EffectKindA0_Fade);
    BOF3_INJECT(EffectKindA0_DrawGlow);
    BOF3_INJECT(EffectKindA0_DrawTrail);
    BOF3_INJECT(EffectKindA0_ClearSparks);
    BOF3_INJECT(EffectKindA0_SpawnSpark);
    BOF3_INJECT(EffectKindA0_StepSparks);
    BOF3_INJECT(EffectKindA0_InitShard);
    BOF3_INJECT(EffectKindA0_DrawShards);
    BOF3_INJECT(EffectKindA0_DrawShard);
    BOF3_INJECT(EffectKindA0_SwapLong);
    BOF3_INJECT(EffectKindA1_Run);
    BOF3_INJECT(EffectKindA1_CaptureSprite);
    BOF3_INJECT(EffectKindA1_ReadBack);
    BOF3_INJECT(EffectKindA1_SplitPixels);
    BOF3_INJECT(EffectKindA1_AimPixels);
    BOF3_INJECT(EffectKindA1_MovePixels);
    BOF3_INJECT(EffectKindA2_Run);
    BOF3_INJECT(EffectKindA2_Start);
    BOF3_INJECT(EffectKindA2_Grow);
    BOF3_INJECT(EffectKindA2_Rewind);
    BOF3_INJECT(EffectKindA2_Shrink);
    BOF3_INJECT(EffectKindA3_Run);
    BOF3_INJECT(EffectKindA3_CaptureSprite);
    BOF3_INJECT(EffectKindA3_ReadBack);
    BOF3_INJECT(EffectKindA3_SplitPixels);
    BOF3_INJECT(EffectKindA3_AimPixels);
    BOF3_INJECT(EffectKindA3_MovePixels);
    BOF3_INJECT(EffectKindA2_DrawRing);
}
