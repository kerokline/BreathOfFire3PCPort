// The two spell overlays no ability loads, round nine group C3
// (docs/magic_c3.md): the PSX's MAGIC002 and MAGIC111.EMI, Magic_Rows rows 2
// and 119. No label in the sibling's names/magic.toml names either: no ability
// id's row byte is 2 or 119. Row 119 is item magic (the item row table
// 0x64B274's category 0, index 32); row 2 is reached by no index inside the
// item or ability tables on the PC (docs/magic_c3.md section 1). The names
// below say what the code does.
//
//   - MAGIC002 0x499D80..0x49A7AD: a kind-3 task that flies a shaded ball from
//     party member 0's screen point towards the source sprite's for ten frames
//     (radius growing), then shatters it for thirty (each facet shrunk round
//     its own centre while the centres spread) - drawn as two mirrored domes
//     of gouraud quads straight in screen space;
//   - MAGIC111 0x4D6110..0x4D67E5: three kind-1 children of 0x57 - a copy of
//     the caster's record that stands in while the others run, a full-screen
//     subtractive wash of pulsing colours, then a full-screen additive flash -
//     and the caster's animation 4 at the end.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_c3.h"

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
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
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

// The scratch words MAGIC111's wash keeps its angles in: SharedScratch
// 0x903850 and 0x903852, read back after every Math_Sin as the original does.
constexpr ::bof3::image::Va kAngleA{SharedScratch_addr};
constexpr ::bof3::image::Va kAngleB{0x903852};

// Capcom's, unnamed, in no group: fild the argument, fsqrt, tail jmp to the
// CRT's _ftol (0x5B9550) - an integer square root, truncated.
constexpr ::bof3::image::CodeVa kSqrt{bof3::addr::Gte_SquareRoot0};
using SqrtFn = int (__cdecl*)(int);
int Sqrt(int v) { return MH_AT(SqrtFn, kSqrt.v)(v); }

// The phase handlers of other units a table holds (docs/magic_c3.md section 3)
// are ours now and named in the tables: the engine's MagicFx_DoneAndFree, S12's
// MagicFx_CountDownRelease / _CountDown2Release, S30's MagicFx_CountUp9By2.

using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }
using BallDrawFn = void (__cdecl*)(int, int, int, int, int, int);

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// --- MAGIC002's ball ---------------------------------------------------------

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

// ===========================================================================
// MAGIC002 (row 2)

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
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr)))->owner, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(ball)));
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

// ===========================================================================
// MAGIC111 (row 119)

// original 0x4D6110: the kind-2 task. A three-entry stack table by +1:
// Magic111_Start, Magic111_Wait, the engine's 0x43FE80.
C3_EXPORT void __cdecl Magic111_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Magic111_Start, bof3::addr::Magic111_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Magic111_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D6140: the task at the owner (direction byte +8, position
// +0x34..+0x3C), +0xB and +9 0, +1 on; three kind-1 children of 0x57, each
// counted in +0xB with +0x80 this task: the first a copy of the acting actor's
// record (0x904B34: party below 3, else enemy - 3; its first 0x80 bytes) with
// +1 and +2 0, kind 1 and parameter 0x57 put back; the second +1 = 1, the third
// +1 = 2. Sounds 0x100 and 0x101; the owner's +0 bit 0x40 set.
C3_EXPORT void __cdecl Magic111_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    unsigned slot = MH_CALL(BattleTask_Create)(1, 0x57) & 0xFFu;
    const unsigned actor = Mem(::bof3::Img(at::kActor))[0];
    const unsigned char* const record =
        actor <= 2 ? Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride) : Mem(::bof3::Img(Battle_Enemies_addr) + (actor - 3) * at::kEnemyStride);
    unsigned char* child = TaskSlot(slot);
    std::memmove(child, record, 0x80);
    unsigned char* self = Sc();
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(child)->owner, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(self)));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x57;
    Inc(self[0xB]);
    for (unsigned kind = 1; kind <= 2; ++kind) {
        slot = MH_CALL(BattleTask_Create)(1, 0x57) & 0xFFu;
        child = TaskSlot(slot);
        self = Sc();
        rv::Store<std::int32_t>(rv::As<records::BattleTask>(child)->owner, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(self)));
        child[1] = static_cast<unsigned char>(kind);
        Inc(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Sound_PlayById)(0x101);
    Owner()[0] |= 0x40;
}

// original 0x4D62C0: once +0xB (the children alive) is 0: animation 4 on the
// caster, the owner's +0 bit 0x40 cleared, the target flag 0x40, +1 on.
C3_EXPORT void __cdecl Magic111_Wait(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Inc(Sc()[1]);
}

// original 0x4D6300: the children's kind-1 task, a jmp through
// Magic111Child_Kinds (three entries) by +1, unchecked.
C3_EXPORT void __cdecl Magic111Child_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::Magic111Double_Run, bof3::addr::Magic111Wash_Run,
                                                bof3::addr::Magic111Flash_Run};
    const unsigned kind = Sc()[1];
    if (kind >= 3) PastTable("Magic111Child_Task", kind, 3);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4D6320: the caster's copy. A three-entry stack table by +2 -
// Magic111Double_Begin, Magic111Double_Wait, BattleFx_FreeTask - then
// Sprite_UpdateScreen while +0 is set.
C3_EXPORT void __cdecl Magic111Double_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::Magic111Double_Begin, bof3::addr::Magic111Double_Wait,
                                                bof3::addr::BattleFx_FreeTask};
    const unsigned step = Sc()[2];
    if (step >= 3) PastTable("Magic111Double_Run", step, 3);
    magic_harness::Phase(kSteps[step])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4D6360: +0x29 = 2, +2 on.
C3_EXPORT void __cdecl Magic111Double_Begin(void) {
    Sc()[0x29] = 2;
    Inc(Sc()[2]);
}

// original 0x4D6380: once the owner's +0xB is 1 or less, it goes down by one
// (0 wraps to 0xFF, as in the original) and +2 on.
C3_EXPORT void __cdecl Magic111Double_Wait(void) {
    unsigned char* const owner = Owner();
    const unsigned char count = owner[0xB];
    if (count > 1) return;
    owner[0xB] = static_cast<unsigned char>(count - 1);
    Inc(Sc()[2]);
}

// original 0x4D63A0: the wash. A call through Magic111Wash_Steps (four
// entries) by +2 - Magic111Wash_Start, BarrierRing_Grow, MagicFx_WaitA,
// MAGIC060's 0x4B1740 -, then while +0 is set: the two angle dwords +0xC and
// +0x10 up by one and the draw (a tail jmp).
C3_EXPORT void __cdecl Magic111Wash_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::Magic111Wash_Start, bof3::addr::BarrierRing_Grow,
                                                bof3::addr::MagicFx_WaitA, bof3::addr::MagicFx_CountDownRelease};
    const unsigned step = Sc()[2];
    if (step >= 4) PastTable("Magic111Wash_Run", step, 4);
    magic_harness::Phase(kSteps[step])();
    if (Sc()[0] == 0) return;
    rv::Store(rv::Current<records::BattleTask>()->step_x, AddU(rv::Current<records::BattleTask>()->step_x, 1));
    rv::Store(rv::Current<records::BattleTask>()->step_z, AddU(rv::Current<records::BattleTask>()->step_z, 1));
    Call0(bof3::addr::Magic111Wash_Draw);
}

// original 0x4D63E0: +0xC = Rand & 0x3F, +0x10 = it + 0x1F (dwords), +9 0,
// +0xA 0x3C, +2 on.
C3_EXPORT void __cdecl Magic111Wash_Start(void) {
    const std::uint32_t r = static_cast<std::uint32_t>(MH_CALL(Rand)());
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(r & 0x3F));
    rv::Store(rv::Current<records::BattleTask>()->step_z, AddU(rv::Current<records::BattleTask>()->step_x, 0x1F));
    Sc()[9] = 0;
    Sc()[0xA] = 0x3C;
    Inc(Sc()[2]);
}

namespace {

// One colour byte of the wash: ((sin(angle) x 6 >> 12) + 8) x +9, both bytes
// (the original's `add al, 8; imul byte [+9]`), +9 read after the call.
unsigned char Pulse(int angle) {
    const int v = MH_CALL(Math_Sin)(angle);
    const auto t = static_cast<unsigned char>((static_cast<int>(static_cast<std::uint32_t>(v) * 6u) >> 12) + 8);
    return static_cast<unsigned char>(t * Sc()[9]);
}
short AngleWord(std::uint32_t cell) { return static_cast<short>(Word(Mem(cell))); }
unsigned SetAngle(std::uint32_t cell, unsigned byte, unsigned add) {
    const unsigned a = ((byte + add) & 0x3F) << 6;
    SetWord(Mem(cell), a);
    return a;
}

// The quad's corners (0, 0)..(319, 239); under DIV-0041 x runs from
// Widescreen_FillX() to 319 + the columns (the PSP's -32..351), 0..319 while
// the fills are unarmed, so the fuzz compares the original's.
void FullScreen(unsigned char* p, unsigned x1, unsigned y1, unsigned x2, unsigned y2, unsigned x3, unsigned y3) {
    constexpr std::uint32_t k239 = 0x436F0000;   // 239.0f
    const auto left = std::bit_cast<std::int32_t>(Widescreen_FillX());
    const auto right = std::bit_cast<std::int32_t>(Widescreen_FillRight(319.0f));
    SetLong(p + 8, left);
    SetLong(p + 0xC, 0);
    SetLong(p + x1, right);
    SetLong(p + y1, 0);
    SetLong(p + x2, left);
    SetLong(p + y2, static_cast<std::int32_t>(k239));
    SetLong(p + x3, right);
    SetLong(p + y3, static_cast<std::int32_t>(k239));
}

}  // namespace

// original 0x4D6420: tpage 0x55 (subtractive) as a draw mode; a
// semi-transparent gouraud quad over the whole screen (0,0)..(319,239), each
// corner's red, green and blue a Pulse of an angle from +0xC / +0x10 (offset
// 0, 0xF, 0x1F, -0x11 by corner, & 0x3F << 6, kept in 0x903850 / 0x903852 and
// read back from there after the first call of each corner, in the original's
// order); then tpage 0x15 as a draw mode.
C3_EXPORT void __cdecl Magic111Wash_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    FullScreen(p, 0x18, 0x1C, 0x28, 0x2C, 0x38, 0x3C);

    unsigned a = SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0);
    SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0);
    unsigned char c0 = Pulse(static_cast<short>(a));
    unsigned char c1 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    unsigned char c2 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    p[4] = c0;
    p[5] = c1;
    p[6] = c2;

    SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0xF);
    unsigned b = SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0xF);
    c0 = Pulse(static_cast<short>(b));
    c1 = Pulse(AngleWord(::bof3::Img(kAngleA.v)));
    c2 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    p[0x14] = c0;
    p[0x15] = c1;
    p[0x16] = c2;

    SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0x1F);
    b = SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0x1F);
    c0 = Pulse(static_cast<short>(b));
    c1 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    c2 = Pulse(AngleWord(::bof3::Img(kAngleA.v)));
    p[0x24] = c0;
    p[0x25] = c1;
    p[0x26] = c2;

    a = SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0x100 - 0x11);
    SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0x100 - 0x11);
    c0 = Pulse(static_cast<short>(a));
    c1 = Pulse(AngleWord(::bof3::Img(kAngleA.v)));
    c2 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    p[0x34] = c0;
    p[0x35] = c1;
    p[0x36] = c2;
    MH_CALL(Gfx_CommitPrim)(3, 0x44);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4D6700: the flash. A call through Magic111Flash_Steps (three
// entries) by +2 - Magic111Flash_Wait, MAGIC131's 0x4E5950, MAGIC060's
// 0x4B18B0 -, then while +0 and +2 are set the draw (a tail jmp).
C3_EXPORT void __cdecl Magic111Flash_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::Magic111Flash_Wait, bof3::addr::MagicFx_CountUp9By2,
                                                bof3::addr::MagicFx_CountDown2Release};
    const unsigned step = Sc()[2];
    if (step >= 3) PastTable("Magic111Flash_Run", step, 3);
    magic_harness::Phase(kSteps[step])();
    if (Sc()[0] == 0 || Sc()[2] == 0) return;
    Call0(bof3::addr::Magic111Flash_Draw);
}

// original 0x4D6730: once the owner's +0xB is 2 or less, +9 0 and +2 on.
C3_EXPORT void __cdecl Magic111Flash_Wait(void) {
    if (Owner()[0xB] > 2) return;
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4D6750: tpage 0x35 (additive) as a draw mode; a
// semi-transparent flat quad over the whole screen, grey +9 x 15 (a byte);
// then tpage 0x15 as a draw mode.
C3_EXPORT void __cdecl Magic111Flash_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyF4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    FullScreen(p, 0x14, 0x18, 0x20, 0x24, 0x2C, 0x30);
    const auto grey = static_cast<unsigned char>(Sc()[9] * 15u);
    p[4] = grey;
    p[5] = grey;
    p[6] = grey;
    MH_CALL(Gfx_CommitPrim)(3, 0x38);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

void MagicC3_Inject() {
    if (bof3::WantsShadow("magic_c3")) magic_c3::SelfTest();
    BOF3_INJECT(Magic002_Task);
    BOF3_INJECT(Magic002_Start);
    BOF3_INJECT(Magic002_Wait);
    BOF3_INJECT(Magic002Ball_Task);
    BOF3_INJECT(Magic002Ball_Fly);
    BOF3_INJECT(Magic002Ball_Shatter);
    BOF3_INJECT(Magic002Ball_Draw);
    BOF3_INJECT(Magic111_Task);
    BOF3_INJECT(Magic111_Start);
    BOF3_INJECT(Magic111_Wait);
    BOF3_INJECT(Magic111Child_Task);
    BOF3_INJECT(Magic111Double_Run);
    BOF3_INJECT(Magic111Double_Begin);
    BOF3_INJECT(Magic111Double_Wait);
    BOF3_INJECT(Magic111Wash_Run);
    BOF3_INJECT(Magic111Wash_Start);
    BOF3_INJECT(Magic111Wash_Draw);
    BOF3_INJECT(Magic111Flash_Run);
    BOF3_INJECT(Magic111Flash_Wait);
    BOF3_INJECT(Magic111Flash_Draw);
}
