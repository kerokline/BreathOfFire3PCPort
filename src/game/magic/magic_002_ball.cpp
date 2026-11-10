// Moved here by tools/move_funcs.py from magic_c3.cpp (docs/source-layout-round.md).
#include "game/magic/magic_002_ball.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// `imul` (the 32-bit product wraps) then an arithmetic shift.
int MulSar(int a, int b, unsigned shift) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> shift;
}

std::int32_t AddU(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// `fild dword` then `fst dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

float AsFloat(int v) { return static_cast<float>(v); }

void PutFloatBits(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

// Capcom's, unnamed, in no group: fild the argument, fsqrt, tail jmp to the
// CRT's _ftol (0x5B9550) - an integer square root, truncated.
constexpr ::bof3::image::CodeVa kSqrt{bof3::addr::Gte_SquareRoot0};

using SqrtFn = int (__cdecl*)(int);

int Sqrt(int v) { return MH_AT(SqrtFn, kSqrt.v)(v); }

using BallDrawFn = void (__cdecl*)(int, int, int, int, int, int);

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The light every facet is shaded by: (-0x93D, -0x93D, -0x93D) (the
// original's `mov di, 0xF6C3`, sign-extended into three registers).
constexpr int kLight = -0x93D;

// `shl eax, 8; sar eax, 0xC` then the low byte.
unsigned char Shade(int root) { return static_cast<unsigned char>(static_cast<int>(static_cast<std::uint32_t>(root) << 8) >> 12); }

int Dot(const short* n, short y) {
    const std::uint32_t d = static_cast<std::uint32_t>(n[0]) * static_cast<std::uint32_t>(kLight) +
                            static_cast<std::uint32_t>(y) * static_cast<std::uint32_t>(kLight) +
                            static_cast<std::uint32_t>(n[2]) * static_cast<std::uint32_t>(kLight);
    return static_cast<int>(d);
}

// A point of the ball: its normal (Gte_VectorNormalS of (x, y, z)) against
// the light, then the same with the normal's y turned over - the upper
// dome's shade and the mirrored lower dome's.
void ShadePoint(int x, int y, int z, unsigned char& upper, unsigned char& lower) {
    const long in[3] = {x, y, z};
    short n[3];
    MH_CALL(Gte_VectorNormalS)(in, n);
    upper = Shade(Sqrt(Dot(n, n[1])));
    n[1] = static_cast<short>(0 - static_cast<int>(n[1]));
    lower = Shade(Sqrt(Dot(n, n[1])));
}

}  // namespace

#define C3_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// Both steps draw with the same six arguments: the 16.16 point's integer
// parts, radius / 2, and the words turn +0x14, spread +0x60 and scale +0x58
// (the originals push the words with whatever the register held above them;
// the draw reads 16 bits of each, and 8 of the turn).
void DrawBall() {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_AT(BallDrawFn, bof3::addr::Magic002Ball_Draw)(ts->step_x >> 16, ts->step_z >> 16, ts->work_18 >> 1,
                                                    static_cast<std::uint16_t>(ts->step_y), Word(s + 0x60), ts->script_pos);
}

}  // namespace

// original 0x499D80: the kind-2 task. A three-entry stack table by +1:
// Magic002_Start, Magic002_Wait, the engine's 0x43FE80.
C3_EXPORT void __cdecl Magic002_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Magic002_Start, bof3::addr::Magic002_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Magic002_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x499DB0: a kind-3 task of parameter 1 (Magic002Ball_Task) at party
// member 0's screen point (+0x2E / +0x30, 16.16), stepping a tenth of the way
// to the source sprite's each frame; radius 0x28, turn 0, spread and scale
// 0x100. The ball's address goes to the current slot's +0x80 (0x93B8C4's, not
// Sprite_Current's) - over this task's own owner pointer. Then +1 on.
C3_EXPORT void __cdecl Magic002_Start(void) {
    const unsigned slot = MH_CALL(BattleTask_Create)(3, 1) & 0xFFu;
    unsigned char* const ball = TaskSlot(slot);
    records::BattleTask* const tball = rv::As<records::BattleTask>(ball);
    const int x0 = static_cast<short>(Word(Mem(::bof3::Img(ObjTrio_addr) + 0x2E)));
    const int y0 = static_cast<short>(Word(Mem(::bof3::Img(ObjTrio_addr) + 0x30)));
    ball[1] = 0;
    tball->step_x = static_cast<std::int32_t>(static_cast<std::uint32_t>(x0) << 16);
    tball->step_z = static_cast<std::int32_t>(static_cast<std::uint32_t>(y0) << 16);
    const unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store(tball->step_y, 0);
    tball->work_18 = 0x28;
    ball[9] = 0;
    const int dx = static_cast<short>(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(source)->screen_x)) - x0;
    tball->work_1c = static_cast<std::int32_t>(static_cast<std::uint32_t>(dx) << 16) / 10;
    const int dy = static_cast<short>(Word(source + 0x30)) - y0;
    rv::Store(tball->work_20, static_cast<std::int32_t>(static_cast<std::uint32_t>(dy) << 16) / 10);
    SetLong(ball + 0x60, 0x100);
    rv::Store<unsigned>(tball->script_pos, 0x100);
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr)))->owner, static_cast<std::int32_t>(Low32(ball)));
    Inc(Sc()[1]);
}

// original 0x499E90: +1 on once the ball's +9 (through the current slot's
// +0x80) reaches 0x28.
C3_EXPORT void __cdecl Magic002_Wait(void) {
    const unsigned char* const slot = Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr));
    const auto* const ball = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(rv::As<records::BattleTask>(slot)->owner));
    if (ball[9] < 0x28) return;
    Inc(Sc()[1]);
}

// original 0x499EB0: the ball's kind-3 task, a two-entry stack table by +1:
// Magic002Ball_Fly, Magic002Ball_Shatter.
C3_EXPORT void __cdecl Magic002Ball_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic002Ball_Fly, bof3::addr::Magic002Ball_Shatter};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Magic002Ball_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x499EE0: turn +0x14 += 0x100, radius +0x18 += 4, the point moved
// by its step (+0xC += +0x1C, +0x10 += +0x20), +9 up; the draw; +1 on once
// +9 is past 10.
C3_EXPORT void __cdecl Magic002Ball_Fly(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    rv::Store(ts->step_y, AddU(ts->step_y, 0x100));
    ts->work_18 = AddU(ts->work_18, 4);
    ts->step_x = AddU(ts->step_x, ts->work_1c);
    ts->step_z = AddU(ts->step_z, ts->work_20);
    Inc(s[9]);
    DrawBall();
    if (Sc()[9] > 10) Inc(Sc()[1]);
}

// original 0x499F70: spread +0x60 += 4, the word scale +0x58 -= 8, turn +0x14
// += 0x20, +9 up; the draw; freed once +9 is past 0x28.
C3_EXPORT void __cdecl Magic002Ball_Shatter(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0x60, AddU(Long(s + 0x60), 4));
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->script_pos, rv::As<records::BattleTask>(s)->script_pos + 0xFFF8u);
    rv::Store(rv::As<records::BattleTask>(s)->step_y, AddU(rv::As<records::BattleTask>(s)->step_y, 0x20));
    Inc(s[9]);
    DrawBall();
    if (Sc()[9] > 0x28) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x499FF0: the ball, in screen space at (x, y) - no GTE transform.
// Tpage Gpu_GetTPage(0x3C0, 0x100, 0, 0) (the arguments in that order: page
// 0, blend 0) as a draw mode. The pole's shade (ten times, as the original
// computes it), then four rings of latitude 0x100..0x400 at radius
// (-sin, -cos) x radius >> 12; each ring's points run from angle (turn & 0xFF)
// in steps of 0x100 while below 0x900, then its far end. Each point is shaded
// twice (ShadePoint). Between a ring and the one before it, one pair of
// gouraud quads a segment, semi-transparent: each quad's corners scaled round
// the segment's centre by scale / 0x100, the centre pushed out by spread /
// 0x100; the upper dome with the upper shades, the lower dome mirrored in y
// with the lower (the first quad's x floats reused). A point's shade is the
// previous ring's at its own index for both of its corners, and this ring's
// at the next index for both of the others - as the original reads them.
C3_EXPORT void __cdecl Magic002Ball_Draw(int x, int y, int radius, int turn, int spread, int scale) {
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0x3C0, 0x100, 0, 0) & 0xFFFFu;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);

    constexpr unsigned kPoints = 10;
    short ring[kPoints] = {};
    short prev[kPoints];
    unsigned char upper[kPoints], lower[kPoints], upper_prev[kPoints], lower_prev[kPoints];
    const int r = static_cast<short>(radius);
    for (unsigned i = 0; i < kPoints; ++i)
        ShadePoint(0, static_cast<short>(0 - static_cast<unsigned>(radius)), 0, upper[i], lower[i]);

    const int x16 = static_cast<short>(x);
    const int y16 = static_cast<short>(y);
    const int sc = static_cast<short>(scale);
    const int sp = static_cast<short>(spread);
    std::int32_t last_c = static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(radius));
    unsigned latitude = 0x100;
    for (unsigned rings = 4; rings != 0; --rings, latitude += 0x100) {
        std::memcpy(lower_prev, lower, sizeof lower);
        std::memcpy(upper_prev, upper, sizeof upper);
        std::memcpy(prev, ring, sizeof ring);
        const std::int32_t cp_full = last_c;
        int v = MH_CALL(Math_Sin)(static_cast<int>(latitude));
        const int s = static_cast<int>(0u - static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(r)) >> 12;
        v = MH_CALL(Math_Cos)(static_cast<int>(latitude));
        const std::int32_t c = static_cast<int>(0u - static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(r)) >> 12;
        last_c = c;
        const int c16 = static_cast<short>(c);
        const int s16 = static_cast<short>(s);
        ring[0] = static_cast<short>(s16);
        ShadePoint(s16, c16, 0, upper[0], lower[0]);
        unsigned k = 1;
        const int start = static_cast<short>(turn & 0xFF);
        if (start < 0x800) {
            unsigned count = (0x8FFu - static_cast<unsigned>(start)) >> 8;
            int angle = start;
            do {
                if (k >= kPoints) bof3::Fatal("Magic002Ball_Draw: point %u past the ring's %u", k, kPoints);
                v = MH_CALL(Math_Cos)(angle);
                const int px = MulSar(v, s16, 12);
                ring[k] = static_cast<short>(px);
                v = MH_CALL(Math_Sin)(angle);
                const int pz = MulSar(v, s16, 12);
                ShadePoint(static_cast<short>(px), c16, static_cast<short>(pz), upper[k], lower[k]);
                angle += 0x100;
                k = static_cast<unsigned char>(k + 1);
            } while (--count != 0);
        }
        if (k >= kPoints) bof3::Fatal("Magic002Ball_Draw: point %u past the ring's %u", k, kPoints);
        ring[k] = static_cast<short>(0 - s);
        ShadePoint(static_cast<short>(0 - s), c16, 0, upper[k], lower[k]);

        const int segments = static_cast<signed char>(k + 1) - 1;
        if (segments <= 0) continue;
        const int cp = static_cast<short>(cp_full);
        const int mid = static_cast<int>(static_cast<std::uint32_t>(cp + c16) << 1) / 4;
        const int oy = MulSar(sp, mid, 8);
        for (int j = 0; j < segments; ++j) {
            const int p0 = prev[j], w0 = ring[j], p1 = prev[j + 1], w1 = ring[j + 1];
            const int centre = (w0 + p0 + p1 + w1) / 4;
            const int xp0 = static_cast<short>(MulSar(p0 - centre, sc, 8));
            const int xw0 = static_cast<short>(MulSar(w0 - centre, sc, 8));
            const int xp1 = static_cast<short>(MulSar(p1 - centre, sc, 8));
            const int xw1 = static_cast<short>(MulSar(w1 - centre, sc, 8));
            const int yp = static_cast<short>(MulSar(cp - mid, sc, 8));
            const int yw = static_cast<short>(MulSar(c16 - mid, sc, 8));
            const int ox = MulSar(sp, centre, 8);

            unsigned char* p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            const float fx0 = AsFloat(AddU(AddU(x16, xp0), ox));
            const float fx1 = AsFloat(AddU(AddU(x16, xp1), ox));
            const float fx2 = AsFloat(AddU(AddU(x16, xw0), ox));
            const float fx3 = AsFloat(AddU(AddU(x16, xw1), ox));
            PutFloatBits(p + 8, fx0);
            PutFloat(p + 0xC, AddU(AddU(y16, yp), oy));
            PutFloatBits(p + 0x18, fx1);
            PutFloat(p + 0x1C, AddU(AddU(y16, yp), oy));
            PutFloatBits(p + 0x28, fx2);
            PutFloat(p + 0x2C, AddU(AddU(y16, yw), oy));
            PutFloatBits(p + 0x38, fx3);
            PutFloat(p + 0x3C, AddU(AddU(y16, yw), oy));
            p[4] = p[5] = p[0x14] = p[0x15] = upper_prev[j];
            p[0x24] = p[0x25] = p[0x34] = p[0x35] = upper[j + 1];
            p[6] = p[0x16] = p[0x26] = p[0x36] = 0;
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            MH_CALL(Gfx_CommitPrim)(1, 0x44);

            p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            PutFloatBits(p + 8, fx0);
            PutFloat(p + 0xC, static_cast<int>(static_cast<std::uint32_t>(y16) - static_cast<std::uint32_t>(yp) - static_cast<std::uint32_t>(oy)));
            PutFloatBits(p + 0x18, fx1);
            PutFloat(p + 0x1C, static_cast<int>(static_cast<std::uint32_t>(y16) - static_cast<std::uint32_t>(yp) - static_cast<std::uint32_t>(oy)));
            PutFloatBits(p + 0x28, fx2);
            PutFloat(p + 0x2C, static_cast<int>(static_cast<std::uint32_t>(y16) - static_cast<std::uint32_t>(yw) - static_cast<std::uint32_t>(oy)));
            PutFloatBits(p + 0x38, fx3);
            PutFloat(p + 0x3C, static_cast<int>(static_cast<std::uint32_t>(y16) - static_cast<std::uint32_t>(yw) - static_cast<std::uint32_t>(oy)));
            p[4] = p[5] = p[0x14] = p[0x15] = lower_prev[j];
            p[0x24] = p[0x25] = p[0x34] = p[0x35] = lower[j + 1];
            p[6] = p[0x16] = p[0x26] = p[0x36] = 0;
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            MH_CALL(Gfx_CommitPrim)(1, 0x44);
        }
    }
}

void Magic002Ball_Inject() {
    BOF3_INJECT(Magic002_Task);
    BOF3_INJECT(Magic002_Start);
    BOF3_INJECT(Magic002_Wait);
    BOF3_INJECT(Magic002Ball_Task);
    BOF3_INJECT(Magic002Ball_Fly);
    BOF3_INJECT(Magic002Ball_Shatter);
    BOF3_INJECT(Magic002Ball_Draw);
}
