// Moved here by tools/move_funcs.py from magic_s36.cpp (docs/source-layout-round.md).
#include "game/magic/intimidate_trail.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s36.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The cells the overlays read beyond the harness's names.
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U32(const unsigned char* at) { return static_cast<std::uint32_t>(Long(at)); }

std::int32_t I32(std::uint32_t v) { return static_cast<std::int32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `add` of two dwords, wrapping.
int Add(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the enemy records by the battle index - 3, unchecked.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Math_Ratan2 of two dword differences, each `fild` into a float.
int Ratan2(std::int32_t a, std::int32_t b) {
    return MH_CALL(Math_Ratan2)(static_cast<float>(a), static_cast<float>(b));
}

// The angle from Sprite_Current to `to` in the effects' plane: Math_Ratan2 of
// (to +0x34 - +0x34, to +0x38 - +0x38), Sprite_Current read now.
int AngleTo(const unsigned char* to) {
    const unsigned char* const s = Sc();
    const std::int32_t dz = I32(U32(to + 0x38) - U32(s + 0x38));
    const std::int32_t dx = I32(U32(to + 0x34) - U32(s + 0x34));
    return Ratan2(dx, dz);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to layer 3.
void DrawModeCommit3(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// Row 26 of Gfx_ClutStrip back from its source (0x812980 / 0x80E980).
void RestoreRow26() {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
}

// The source sprite's point as MagicFx_StepTowardPoint takes it: (x sar 9) -
// 0x4000, (z sar 9) - 0x4000, (y + lift) sar 0x11; the fourth word the
// original pushes is its own uninitialised stack, which the callee never reads.
void StepTowardSource(const unsigned char* src, std::uint32_t lift, int speed) {
    const std::uint32_t x = static_cast<std::uint32_t>((rv::As<records::SpriteHead>(src)->x >> 9) - 0x4000);
    const std::uint32_t z = static_cast<std::uint32_t>((rv::As<records::SpriteHead>(src)->z >> 9) - 0x4000);
    const std::uint32_t y = static_cast<std::uint32_t>(I32(U32(src + 0x3C) + lift) >> 0x11);
    MH_CALL(MagicFx_StepTowardPoint)(x, z, y, 0, speed);
}

// Sprite_Current's +0x10 = (+0x10 & 0xFFF) - (+0xC & 0xFFF), then its
// absolute value (`not / inc`): how far the heading turned this frame. True
// when it is above 0x600 and below 0xA00 (it went past the target).
bool TurnedPast() {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = I32((U32(s + 0x10) & 0xFFF) - (U32(s + 0xC) & 0xFFF));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        const std::int32_t v = ts->step_z;
        if (v < 0) ts->step_z = I32(0u - static_cast<std::uint32_t>(v));
    }
    const std::int32_t v = rv::Current<records::BattleTask>()->step_z;
    return v > 0x600 && v < 0xA00;
}

// One point of a trail: IntimidateTrail_Points + ((trail << 5) + k) x 4, the
// x word and the y word; neither index is checked by the originals.
unsigned char* TrailPoint(unsigned trail, unsigned k) { return Mem(::bof3::Img(IntimidateTrail_Points_addr) + ((trail << 5) + k) * 4); }

}  // namespace

#define S36_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The trail's points +0xB .. +0xA - 1 moved one on (from the end), then
// (x, y) at +0xB.
void TrailShift(unsigned x, unsigned y) {
    const unsigned char* const s = Sc();
    int k = static_cast<int>(s[0xA]) - 1;
    while (static_cast<int>(s[0xB]) <= k) {
        SetWord(TrailPoint(s[4], static_cast<unsigned>(k) + 1), Word(TrailPoint(s[4], static_cast<unsigned>(k))));
        SetWord(TrailPoint(s[4], static_cast<unsigned>(k) + 1) + 2, Word(TrailPoint(s[4], static_cast<unsigned>(k)) + 2));
        --k;
    }
    SetWord(TrailPoint(s[4], s[0xB]), x);
    SetWord(TrailPoint(s[4], s[0xB]) + 2, y);
}

// The ribbon's head: from the trail's point +0xB past any gap (x -1, the scan
// unbounded), the first point to (0x90385C, 0x90385E), the next to (0x903858,
// 0x90385A), the angle between them (Math_Ratan2(dx, dy)) to 0x903852; then,
// Sprite_Current read again, the first point to (0x903858, 0x90385A).
// Answers the index after it.
unsigned RibbonHead() {
    const unsigned char* s = Sc();
    unsigned k = s[0xB];
    for (const unsigned char* q = TrailPoint(s[4], k); Word(q) == 0xFFFF; q += 4) ++k;
    const short x0 = S16(TrailPoint(s[4], k)), y0 = S16(TrailPoint(s[4], k) + 2);
    SetSW(0xC, static_cast<unsigned short>(x0));
    SetSW(0xE, static_cast<unsigned short>(y0));
    const short x1 = S16(TrailPoint(s[4], k + 1)), y1 = S16(TrailPoint(s[4], k + 1) + 2);
    SetSW(8, static_cast<unsigned short>(x1));
    SetSW(0xA, static_cast<unsigned short>(y1));
    SetSW(2, static_cast<unsigned>(Ratan2(x1 - x0, y1 - y0)));
    s = Sc();
    SetSW(8, Word(TrailPoint(s[4], k)));
    SetSW(0xA, Word(TrailPoint(s[4], k) + 2));
    return k + 1;
}

// One side of a quad: the point at the scratch pair (x word, y word) offset
// by the radius word 0x903850 at (0x903852 + turn) & 0xFFF, into the
// floats at p + at.
void Offset(unsigned char* p, unsigned at, unsigned xw, unsigned yw, unsigned turn) {
    const unsigned a = (SW(2) + turn) & 0xFFF;
    SetSW(4, a);
    int v = MH_CALL(Math_Sin)(static_cast<short>(a));
    PutFloat(p + at, Add(Mul12(v, SS(0)), SS(xw)));
    v = MH_CALL(Math_Cos)(SS(4));
    PutFloat(p + at + 4, Add(Mul12(v, SS(0)), SS(yw)));
}

// The next point of the ribbon: the last one to (0x90385C, 0x90385E), point k
// to (0x903858, 0x90385A); a gouraud quad at Gfx_PacketNext.
unsigned char* RibbonStep(unsigned k) {
    SetSW(0xC, SW(8));
    SetSW(0xE, SW(0xA));
    const unsigned char* const s = Sc();
    unsigned char* const p = Gfx_PacketNext;
    SetSW(8, Word(TrailPoint(s[4], k)));
    SetSW(0xA, Word(TrailPoint(s[4], k) + 2));
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    return p;
}

// The angle from the last point to this one, to 0x903852.
void RibbonTurn() { SetSW(2, static_cast<unsigned>(Ratan2(SS(8) - SS(0xC), SS(0xA) - SS(0xE)))); }

// The thin ribbon's shades: `first` to 0x903856, its low byte red and green
// of corners 0 and 2 (blue 1); then `second`, the same for corners 1 and 3.
void ThinShade(unsigned char* p, unsigned first, unsigned second) {
    SetSW(6, first);
    p[4] = static_cast<unsigned char>(first);
    p[5] = SB(6);
    p[6] = 1;
    p[0x24] = SB(6);
    p[0x25] = SB(6);
    p[0x26] = 1;
    SetSW(6, second);
    p[0x14] = static_cast<unsigned char>(second);
    p[0x15] = SB(6);
    p[0x16] = 1;
    p[0x34] = SB(6);
    p[0x35] = SB(6);
    p[0x36] = 1;
}

// The wide ribbon's: corners 0 and 1 (the offset edge) 1; `first` to
// 0x903856 and its low byte into corner 2, `second` into corner 3.
void WideShade(unsigned char* p, unsigned first, unsigned second) {
    SetSW(6, first);
    for (unsigned c = 0; c < 3; ++c) p[4 + c] = 1;
    for (unsigned c = 0; c < 3; ++c) p[0x24 + c] = SB(6);
    SetSW(6, second);
    for (unsigned c = 0; c < 3; ++c) p[0x14 + c] = 1;
    for (unsigned c = 0; c < 3; ++c) p[0x34 + c] = SB(6);
}

// One pass of trail 0's ribbon: a quad from each point to 0xC to one side
// (`turn` 0x400 or -0x400) of the line through it, the line's own edge shaded
// (`near` - step k, `far` - step k), the offset edge 1.
void WidePass(unsigned turn, unsigned near, unsigned far, unsigned step) {
    unsigned k = RibbonHead();
    if (static_cast<int>(k) >= static_cast<int>(Sc()[0xA])) return;
    do {
        unsigned char* const p = RibbonStep(k);
        Offset(p, 8, 0xC, 0xE, turn);
        PutFloat(p + 0x28, SS(0xC));
        PutFloat(p + 0x2C, SS(0xE));
        RibbonTurn();
        Offset(p, 0x18, 8, 0xA, turn);
        PutFloat(p + 0x38, SS(8));
        PutFloat(p + 0x3C, SS(0xA));
        const unsigned t = 0u - step * k;
        WideShade(p, t + near, t + far);
        MH_CALL(Gfx_CommitPrim)(3, 0x44);
        ++k;
    } while (static_cast<int>(k) < static_cast<int>(Sc()[0xA]));
}

}  // namespace

// original 0x4F3540: the kind-2 task. A two-entry stack table by +1:
// Intimidate_Start, BattleFx_Finish.
S36_EXPORT void __cdecl Intimidate_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Intimidate_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Intimidate_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4F3570: the owner's direction and point; +0xB 0, +1 on; two
// trails (kind 1, 0x18, +1 0, +4 0 and 1), each with +0x80 this task and
// counted in +0xB; row 26 of the CLUT strip back; sound 0x100.
S36_EXPORT void __cdecl Intimidate_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned slot = NewTask(0x18);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = I32(Key(s));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(i);
        Inc(s[0xB]);
    }
    RestoreRow26();
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F3640: the children's kind-1 task, a jmp through
// IntimidateChild_Kinds (two entries: a trail, the burst) by +1, unchecked.
S36_EXPORT void __cdecl IntimidateChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::IntimidateTrail_Run, bof3::addr::IntimidateBurst_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("IntimidateChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4F3660: a call through IntimidateTrail_Steps (four entries:
// _Start, _Fly, _Grow, _Fade) by +2; then while +0 and +2 are set, trail 1
// (+4 not 0) draws the thin ribbon, trail 0 the wide one.
S36_EXPORT void __cdecl IntimidateTrail_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::IntimidateTrail_Start, bof3::addr::IntimidateTrail_Fly,
                                                bof3::addr::IntimidateTrail_Grow, bof3::addr::IntimidateTrail_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("IntimidateTrail_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(s[4] != 0 ? bof3::addr::IntimidateTrail_DrawThin : bof3::addr::IntimidateTrail_DrawWide);
}

// original 0x4F36B0: the owner's direction; an offset by the acting actor (a
// party member (0x4000, 0, 0xC00000); an enemy whose record +0x8C is 0x61
// (0x38000, 0, 0), 0x6C (0x20000 or, facing bit 1 clear, 0x30000, 0,
// 0x800000), any other (0x10000, 0, 0x800000)) turned by the direction and
// added to the owner's point; the heading the angle to the source sprite,
// +0x10 0; the screen point, its trail's first two points; +0xB 0, +9 0x10,
// +0xA 1, +2 on.
S36_EXPORT void __cdecl IntimidateTrail_Start(void) {
    Sc()[8] = Owner()[8];
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    std::uint32_t dx, dy;
    const unsigned actor = ActorIndex();
    if (actor < 3) {
        dx = 0x4000;
        dy = 0xC00000;
    } else {
        const unsigned char kind = EnemyRecord(actor)[0x8C];
        dy = 0x800000;
        if (kind == 0x61) {
            dx = 0x38000;
            dy = 0;
        } else if (kind == 0x6C) {
            dx = Sc()[8] & 2 ? 0x20000u : 0x30000u;
        } else {
            dx = 0x10000;
        }
    }
    rv::Store(rv::Current<records::BattleTask>()->step_x, I32(dx));
    rv::Current<records::BattleTask>()->step_z = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_y, I32(dy));
    Turn(Sc());
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const s = Sc();
        SetLong(s + 0x34 + 4 * k, I32(U32(Owner() + 0x34 + 4 * k) + U32(s + 0xC + 4 * k)));
    }
    const int angle = AngleTo(src);
    rv::Current<records::BattleTask>()->step_x = angle;
    rv::Current<records::BattleTask>()->step_z = 0;
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sc();
    SetWord(TrailPoint(s[4], 0), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetWord(TrailPoint(s[4], 0) + 2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    SetWord(TrailPoint(s[4], 1), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetWord(TrailPoint(s[4], 1) + 2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    s[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4F38B0: +0xA up below 8; a step (0xC0) toward the source sprite's
// point 0x800000 higher; the heading as the core's; at the source, or turned
// past it, the point set on the source's x / z and +2 on. Then the screen
// point, pushed onto the trail; trail 0 flags the target 0x10 every frame but
// the first step's; on reaching step 2 the burst (kind 1, 0x18, +1 1, the
// owner's child, at this point), counted in the owner's +0xB.
S36_EXPORT void __cdecl IntimidateTrail_Fly(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] < 8) Inc(s[0xA]);
    }
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    StepTowardSource(src, 0x800000u, 0xC0);
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = ts->step_x;
    }
    const int angle = AngleTo(src);
    rv::Current<records::BattleTask>()->step_x = angle;
    bool arrived;
    if (MH_CALL(MagicFx_NearSprite)(src, 0x8000) != 0) {
        arrived = true;
    } else {
        arrived = TurnedPast();
    }
    if (arrived) {
        rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
        rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
        Inc(Sc()[2]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::IntimidateTrail_Push);
    {
        const unsigned char* const s = Sc();
        if (s[2] != 1 && s[4] == 0) MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    }
    if (Sc()[2] != 2) return;
    const unsigned slot = NewTask(0x18);
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    unsigned char* const owner = Owner();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = I32(Key(owner));
    child[1] = 1;
    tchild->x = ts->x;
    tchild->z = ts->z;
    rv::Store(tchild->height, ts->height);
    Inc(owner[0xB]);
}

// original 0x4F3A80: below +0xA 8, +0xB and +0xA up, the screen point pushed
// onto the trail; once +0xA is 8 (at entry, or after the push) +9 8 and +2 on.
S36_EXPORT void __cdecl IntimidateTrail_Grow(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] >= 8) {
            if (s[0xA] == 8) {
                s[9] = 8;
                Inc(Sc()[2]);
            }
            return;
        }
        Inc(s[0xB]);
        Inc(Sc()[0xA]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::IntimidateTrail_Push);
    unsigned char* const s = Sc();
    if (s[0xA] != 8) return;
    s[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F3AC0: a gap pushed onto the trail; +9 down, at 0 the owner's
// +0xB down and the task freed.
S36_EXPORT void __cdecl IntimidateTrail_Fade(void) {
    Call0(bof3::addr::IntimidateTrail_PushGap);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F3AF0: trail 1's ribbon - a gouraud quad between each pair of
// its points from +0xB (past any gap) to +0xA, 2 either side of the line
// between them, the near ends shaded (0x6D - 12 k, 0x6D - 12 k, 1) and the far
// (0x61 - 12 k, ..., 1); between two draw-mode packets (0x35, 0x15).
S36_EXPORT void __cdecl IntimidateTrail_DrawThin(void) {
    DrawModeCommit3(0x35);
    SetSW(0, 2);
    unsigned k = RibbonHead();
    if (static_cast<int>(k) < static_cast<int>(Sc()[0xA])) {
        do {
            unsigned char* const p = RibbonStep(k);
            Offset(p, 8, 0xC, 0xE, 0x400);
            Offset(p, 0x28, 0xC, 0xE, 0xFFFFFC00u);
            RibbonTurn();
            Offset(p, 0x18, 8, 0xA, 0x400);
            Offset(p, 0x38, 8, 0xA, 0xFFFFFC00u);
            const unsigned t = 0u - 12u * k;
            ThinShade(p, t + 0x6D, t + 0x61);
            MH_CALL(Gfx_CommitPrim)(3, 0x44);
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(Sc()[0xA]));
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4F3F40: trail 0's ribbon - two passes over its points, 0xC to
// either side of the line: first the +0x400 side (shades 0x6D and 0x61 less
// 12 k), then the -0x400 side (0x91 and 0x81 less 16 k); between two
// draw-mode packets (0x35, 0x15).
S36_EXPORT void __cdecl IntimidateTrail_DrawWide(void) {
    DrawModeCommit3(0x35);
    SetSW(0, 0xC);
    WidePass(0x400, 0x6D, 0x61, 12);
    WidePass(0xFFFFFC00u, 0x91, 0x81, 16);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4F4640: the screen point (+0x2E, +0x30) pushed onto the trail at
// +0xB, the points to +0xA moved on.
S36_EXPORT void __cdecl IntimidateTrail_Push(void) {
    const unsigned char* const s = Sc();
    TrailShift(static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
}

// original 0x4F46D0: a gap (-1, -1) pushed onto the trail the same way.
S36_EXPORT void __cdecl IntimidateTrail_PushGap(void) { TrailShift(0xFFFF, 0xFFFF); }

// original 0x4F4760: the burst's kind-1 task, with the frame-offset table the
// effects' (0x8E3580) round it: a call through IntimidateBurst_Steps (four
// entries: _Start, _Rise, _Play, _Fade) by +2; then while +0 is set the
// screen point, the disc, and before step 3 the sprite's screen update.
S36_EXPORT void __cdecl IntimidateBurst_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::IntimidateBurst_Start, bof3::addr::IntimidateBurst_Rise,
                                                bof3::addr::IntimidateBurst_Play, bof3::addr::IntimidateBurst_Fade};
    const unsigned phase = Sc()[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    if (phase >= 4) PastTable("IntimidateBurst_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        Call0(bof3::addr::IntimidateBurst_DrawDisc);
        if (Sc()[2] < 3) MH_CALL(Sprite_UpdateScreen)();
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4F47B0: the sprite set up (+0x25 0x1D, +0x26 0, +0x27 0xA0, +0x28
// 0, +0x24 4, +0x5C..+0x5F 0, +0x2A 0, +0x29 3, word +0x2C 0, +0x2B 1),
// animation 0; +9 0, +2 on.
S36_EXPORT void __cdecl IntimidateBurst_Start(void) {
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x27] = 0xA0;
    s[0x28] = 0;
    s[0x24] = 4;
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    s[0x5C] = 0;
    s[0x2A] = 0;
    s[0x29] = 3;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
    s[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4F4850: the script ticked; +9 up by 4, at 0x10 +2 on.
S36_EXPORT void __cdecl IntimidateBurst_Rise(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4F4880: the script ticked twice; +2 on when the second tick
// reports its end.
S36_EXPORT void __cdecl IntimidateBurst_Play(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) Inc(Sc()[2]);
}

// original 0x4F48A0: +9 down by 4; at 0 the owner's +0xB down and the task
// freed.
S36_EXPORT void __cdecl IntimidateBurst_Fade(void) {
    AddB(Sc()[9], 0xFC);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F48D0: the burst's disc - eight triangles of radius (Rand & 3) +
// 0x1C round the screen point (read again after every call), the centre
// shaded +9 x 6, the rim 1; after a draw-mode packet (0x35).
S36_EXPORT void __cdecl IntimidateBurst_DrawDisc(void) {
    DrawModeCommit3(0x35);
    const std::uint32_t r = RandCall();
    SetSW(0, (r & 3) + 0x1C);
    SetSW(6, Sc()[9] * 6u);
    for (int a = 0; a < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        int v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x18, Add(Mul12(v, SS(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x1C, Add(Mul12(v, SS(0)), S16(Sc() + 0x30)));
        a += 0x200;
        v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x28, Add(Mul12(v, SS(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x2C, Add(Mul12(v, SS(0)), S16(Sc() + 0x30)));
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = SB(6);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
}

void IntimidateTrail_Inject() {
    BOF3_INJECT(Intimidate_Task);
    BOF3_INJECT(Intimidate_Start);
    BOF3_INJECT(IntimidateChild_Task);
    BOF3_INJECT(IntimidateTrail_Run);
    BOF3_INJECT(IntimidateTrail_Start);
    BOF3_INJECT(IntimidateTrail_Fly);
    BOF3_INJECT(IntimidateTrail_Grow);
    BOF3_INJECT(IntimidateTrail_Fade);
    BOF3_INJECT(IntimidateTrail_DrawThin);
    BOF3_INJECT(IntimidateTrail_DrawWide);
    BOF3_INJECT(IntimidateTrail_Push);
    BOF3_INJECT(IntimidateTrail_PushGap);
    BOF3_INJECT(IntimidateBurst_Run);
    BOF3_INJECT(IntimidateBurst_Start);
    BOF3_INJECT(IntimidateBurst_Rise);
    BOF3_INJECT(IntimidateBurst_Play);
    BOF3_INJECT(IntimidateBurst_Fade);
    BOF3_INJECT(IntimidateBurst_DrawDisc);
}
