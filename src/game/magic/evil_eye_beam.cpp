// Moved here by tools/move_funcs.py from magic_s08.cpp (docs/source-layout-round.md).
#include "game/magic/evil_eye_beam.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s08.h"
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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The enemy records by battle index - 3, unchecked, as the originals index them.
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

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The task's position +0x34 / +0x38 / +0x3C from the owner's.
void OwnerPosition() {
    const unsigned char* const o = Owner();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->x = rv::As<records::SpriteHead>(o)->x;
    ts->z = rv::As<records::SpriteHead>(o)->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(o)->height);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void LinkAtTask(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy, size);
}

int Ratan2(int y, int x) { return MH_CALL(Math_Ratan2)(static_cast<float>(y), static_cast<float>(x)); }

// EvilEyeBeam_Trails: point j of beam `beam` (+4 of the task), x then y.
unsigned char* Trail(unsigned beam, int j) {
    return Mem(::bof3::Img(EvilEyeBeam_Trails_addr) + static_cast<std::uint32_t>((static_cast<int>(beam) * 32 + j) * 4));
}

}  // namespace

#define S08_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The erased points skipped, the first segment's heading into scratch word 2,
// the first point in scratch 8 / 0xA; j the point after it.
int TrailStart(const unsigned char* s) {
    int j = s[0xB];
    const unsigned beam = s[4];
    while (Word(Trail(beam, j)) == 0xFFFF) ++j;
    const unsigned x0 = Word(Trail(s[4], j)), y0 = Word(Trail(s[4], j) + 2);
    const unsigned x1 = Word(Trail(s[4], j) + 4), y1 = Word(Trail(s[4], j) + 6);
    SetSW(0xC, x0);
    SetSW(0xE, y0);
    SetSW(8, x1);
    SetSW(0xA, y1);
    const int heading = Ratan2(static_cast<short>(x1) - static_cast<short>(x0), static_cast<short>(y1) - static_cast<short>(y0));
    SetSW(2, static_cast<unsigned>(heading));
    const unsigned char* const t = Sc();
    SetSW(8, Word(Trail(t[4], j)));
    SetSW(0xA, Word(Trail(t[4], j) + 2));
    return j + 1;
}

// One corner pair: (x, y) + the width along `angle` into +xo / +yo.
void Corner(unsigned char* p, unsigned xo, unsigned angle, unsigned bx, unsigned by) {
    SetSW(4, angle & 0xFFF);
    int v = MH_CALL(Math_Sin)(static_cast<short>(angle & 0xFFF));
    PutFloat(p + xo, Mul12(v, SS(0)) + SS(bx));
    v = MH_CALL(Math_Cos)(SS(4));
    PutFloat(p + xo + 4, Mul12(v, SS(0)) + SS(by));
}

int SegmentHeading() {
    return Ratan2(SS(8) - SS(0xC), SS(0xA) - SS(0xE));
}

void TrailShift(const unsigned char* s) {
    for (int i = static_cast<int>(s[0xA]) - 1; i >= static_cast<int>(s[0xB]); --i) {
        SetWord(Trail(s[4], i + 1), Word(Trail(s[4], i)));
        SetWord(Trail(s[4], i + 1) + 2, Word(Trail(s[4], i) + 2));
    }
}

}  // namespace

// original 0x4A8250: the kind-2 task. A two-entry stack table by +1:
// EvilEye_Start, BattleFx_Finish.
S08_EXPORT void __cdecl EvilEye_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::EvilEye_Start, bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 2, Sc()[1], "EvilEye_Task");
}

// original 0x4A8280: the owner's direction and position; +0xB 0, +1 on; two
// beams (kind 1, 6; +1 0, +4 the beam 0 / 1) owned by this task and counted
// in +0xB; row 26's third CLUT entries 1..15 from their source with STP,
// entry 0 cleared; Gfx_ClutStripDirty; sound 0x100.
S08_EXPORT void __cdecl EvilEye_Start(void) {
    Sc()[8] = Owner()[8];
    OwnerPosition();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned beam = 0; beam < 2; ++beam) {
        const unsigned slot = NewTask(6);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(beam);
        Inc(self[0xB]);
    }
    for (unsigned k = 0x1A21; k < 0x1A30; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A20] = 0;
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A8360: the children's kind-1 task, a jmp through
// EvilEyeChild_Kinds (two entries: the beam, the spark) by +1.
S08_EXPORT void __cdecl EvilEyeChild_Task(void) { CellCall(::bof3::Img(0x65A924), 2, Sc()[1], "EvilEyeChild_Task"); }

// original 0x4A8380: a call through EvilEyeBeam_Steps (four entries) by +2;
// then while +0 and +2 are set, the trail drawn: thin for beam 1 (+4 not 0),
// wide for beam 0.
S08_EXPORT void __cdecl EvilEyeBeam_Run(void) {
    CellCall(::bof3::Img(0x65A92C), 4, Sc()[2], "EvilEyeBeam_Run");
    const unsigned char* const s = Sc();
    const bool thin = s[4] != 0;
    if (s[0] == 0 || s[2] == 0) return;
    Call0(thin ? bof3::addr::EvilEyeBeam_DrawThin : bof3::addr::EvilEyeBeam_DrawWide);
}

// original 0x4A83D0: the owner's direction; an offset by the acting actor (a
// party member (0x4000, 0), height 0x1000000; an enemy by its record's +0x8C:
// 0x9A (0x10000, 0) and 0x2000000, 0xB5 (-0xE000, 0) and 0x3800000, any other
// (0x2000, 0) and 0x1C00000) in +0xC / +0x10 / +0x14, turned by the direction,
// added to the owner's position; +0xC the angle to the source sprite
// (Math_Ratan2 of dx, dz), +0x10 0; the screen point, twice into the trail's
// first two points; +0xB 0, +9 0x10, +0xA 1, +2 on.
S08_EXPORT void __cdecl EvilEyeBeam_Start(void) {
    const unsigned char* const src = Source();
    Sc()[8] = Owner()[8];
    const unsigned actor = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFFu;
    std::uint32_t dx, height;
    if (actor < 3) {
        dx = 0x4000;
        height = 0x1000000;
    } else {
        const unsigned kind = EnemyRecord(actor)[0x8C];
        if (kind == 0x9A) {
            dx = 0x10000;
            height = 0x2000000;
        } else if (kind == 0xB5) {
            dx = 0xFFFF2000u;
            height = 0x3800000;
        } else {
            dx = 0x2000;
            height = 0x1C00000;
        }
    }
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(dx));
    rv::Current<records::BattleTask>()->step_z = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_y, static_cast<std::int32_t>(height));
    Turn(Sc());
    {
        const unsigned char* const o = Owner();
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->x) +
                                                    static_cast<std::uint32_t>(ts->step_x));
        ts->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->z) +
                                                    static_cast<std::uint32_t>(ts->step_z));
        rv::Store(ts->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->height) +
                                                    static_cast<std::uint32_t>(ts->step_y)));
    }
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const int ddz = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->z) - static_cast<std::uint32_t>(ts->z));
        const int ddx = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->x) - static_cast<std::uint32_t>(ts->x));
        const int angle = Ratan2(ddx, ddz);
        rv::Current<records::BattleTask>()->step_x = angle;
    }
    rv::Current<records::BattleTask>()->step_z = 0;
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sc();
    SetWord(Trail(s[4], 0), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetWord(Trail(s[4], 0) + 2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    SetWord(Trail(s[4], 1), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetWord(Trail(s[4], 1) + 2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    s[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4A85C0: +0xA (the trail's length) up to 0x10; a step toward the
// source sprite (MagicFx_StepTowardPoint, speed 0xC0), the old heading +0xC
// kept in +0x10 and the new one (Math_Ratan2 of dx, dz) in +0xC. Snapped onto
// the source, +2 on, when MagicFx_NearSprite(source, 0x8000) answers, or when
// the turn |(+0x10 & 0xFFF) - (+0xC & 0xFFF)| is between 0x600 and 0xA00
// (both open). Then the screen point, the trail recorded, and target flags
// 0x10 from beam 0 unless +2 is 1.
S08_EXPORT void __cdecl EvilEyeBeam_Seek(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] < 0x10) Inc(s[0xA]);
    }
    unsigned char* const src = Source();
    {
        const std::uint32_t x = static_cast<std::uint32_t>((rv::As<records::SpriteHead>(src)->x >> 9) - 0x4000);
        const std::uint32_t z = static_cast<std::uint32_t>((rv::As<records::SpriteHead>(src)->z >> 9) - 0x4000);
        const std::uint32_t y =
            static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->height) + 0x800000u) >> 0x11);
        MH_CALL(MagicFx_StepTowardPoint)(x, z, y, 0, 0xC0);
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = ts->step_x;
        const int ddz = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->z) - static_cast<std::uint32_t>(ts->z));
        const int ddx = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->x) - static_cast<std::uint32_t>(ts->x));
        const int angle = Ratan2(ddx, ddz);
        rv::Current<records::BattleTask>()->step_x = angle;
    }
    bool snap;
    if (MH_CALL(MagicFx_NearSprite)(src, 0x8000) != 0) {
        snap = true;
    } else {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = static_cast<std::int32_t>((static_cast<std::uint32_t>(ts->step_z) & 0xFFF) -
                                                    (static_cast<std::uint32_t>(ts->step_x) & 0xFFF));
        if (ts->step_z < 0) ts->step_z = static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(ts->step_z));
        const std::int32_t turn = ts->step_z;
        snap = turn > 0x600 && turn < 0xA00;
    }
    if (snap) {
        rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
        rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
        Inc(Sc()[2]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::EvilEyeBeam_Record);
    const unsigned char* const s = Sc();
    if (s[2] == 1 || s[4] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
}

// original 0x4A8730: while the trail is shorter than 0x10, +0xB and +0xA up,
// the screen point and the trail recorded; every fourth frame a spark (kind 1,
// 6; +1 1) at the beam's position owned by the beam's owner (whose +0xB
// counts it), unless BattleTask_Create answers 0xFF; at a length of 0x10 +9
// 0x10 and +2 on.
S08_EXPORT void __cdecl EvilEyeBeam_Trail(void) {
    if (Sc()[0xA] < 0x10) {
        Inc(Sc()[0xB]);
        Inc(Sc()[0xA]);
        MH_CALL(BattleActor_UpdateScreenXY)();
        Call0(bof3::addr::EvilEyeBeam_Record);
    }
    if ((Frame_Counter & 3) == 0) {
        const unsigned slot = NewTask(6);
        if (slot != 0xFF) {
            const unsigned char* const s = Sc();
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            unsigned char* const o = Owner();
            unsigned char* const child = TaskSlot(slot);
            records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
            tchild->owner = static_cast<std::int32_t>(Key(o));
            child[1] = 1;
            tchild->x = ts->x;
            tchild->z = ts->z;
            rv::Store(tchild->height, ts->height);
            Inc(o[0xB]);
        }
    }
    unsigned char* const s = Sc();
    if (s[0xA] != 0x10) return;
    s[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A87F0: the trail erased from its head (EvilEyeBeam_Erase), +9
// down; at 0 the owner's +0xB down and the task freed.
S08_EXPORT void __cdecl EvilEyeBeam_End(void) {
    Call0(bof3::addr::EvilEyeBeam_Erase);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A8820: beam 1's trail, 2 wide, both edges pushed out; the near
// end shaded 0xCD - 12 j in blue, the far end 0xC1 - 12 j (scratch word 4
// left at 0x61 - 6 j).
S08_EXPORT void __cdecl EvilEyeBeam_DrawThin(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* s = Sc();
    SetSW(0, 2);
    int j = TrailStart(s);
    s = Sc();
    if (j < s[0xA]) {
        do {
            SetSW(0xC, SW(8));
            SetSW(0xE, SW(0xA));
            unsigned char* const p = Gfx_PacketNext;
            SetSW(8, Word(Trail(s[4], j)));
            SetSW(0xA, Word(Trail(s[4], j) + 2));
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Corner(p, 8, SW(2) + 0x400u, 0xC, 0xE);
            Corner(p, 0x28, SW(2) - 0x400u, 0xC, 0xE);
            const int heading = SegmentHeading();
            SetSW(2, static_cast<unsigned>(heading));
            Corner(p, 0x18, static_cast<unsigned>(heading) + 0x400u, 8, 0xA);
            Corner(p, 0x38, SW(2) - 0x400u, 8, 0xA);
            const unsigned twelve = static_cast<unsigned>(12 * j);
            SetSW(6, 0xCD - twelve);
            p[4] = 1;
            p[5] = 1;
            p[6] = SB(6);
            p[0x24] = 1;
            p[0x25] = 1;
            p[0x26] = SB(6);
            SetSW(4, 0x61 - static_cast<unsigned>(6 * j));
            SetSW(6, 0xC1 - twelve);
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = SB(6);
            p[0x34] = 1;
            p[0x35] = 1;
            p[0x36] = SB(6);
            MH_CALL(Gfx_CommitPrim)(3, 0x44);
            s = Sc();
            ++j;
        } while (j < s[0xA]);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A8C70: beam 0's trail, 0xC wide, drawn twice: the left edge
// (heading + 0x400) against the trail itself, shaded 0x67 - 6 j near and
// 0x61 - 6 j far in blue; then the right edge (heading - 0x400), 0x89 - 8 j
// and 0x81 - 8 j.
S08_EXPORT void __cdecl EvilEyeBeam_DrawWide(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* s = Sc();
    SetSW(0, 0xC);
    for (int pass = 0; pass < 2; ++pass) {
        const unsigned turn = pass == 0 ? 0x400u : 0u - 0x400u;
        int j = TrailStart(s);
        s = Sc();
        if (j >= s[0xA]) continue;
        do {
            SetSW(0xC, SW(8));
            SetSW(0xE, SW(0xA));
            unsigned char* const p = Gfx_PacketNext;
            SetSW(8, Word(Trail(s[4], j)));
            SetSW(0xA, Word(Trail(s[4], j) + 2));
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Corner(p, 8, SW(2) + turn, 0xC, 0xE);
            PutFloat(p + 0x28, SS(0xC));
            PutFloat(p + 0x2C, SS(0xE));
            const int heading = SegmentHeading();
            SetSW(2, static_cast<unsigned>(heading));
            Corner(p, 0x18, static_cast<unsigned>(heading) + turn, 8, 0xA);
            PutFloat(p + 0x38, SS(8));
            PutFloat(p + 0x3C, SS(0xA));
            const unsigned near = pass == 0 ? 0x67 - static_cast<unsigned>(6 * j) : 0x89 - static_cast<unsigned>(8 * j);
            const unsigned far = pass == 0 ? 0x61 - static_cast<unsigned>(6 * j) : 0x81 - static_cast<unsigned>(8 * j);
            SetSW(6, near);
            p[4] = 1;
            p[5] = 1;
            p[6] = 1;
            p[0x25] = 1;
            p[0x24] = SB(6);
            p[0x26] = SB(6);
            SetSW(6, far);
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = 1;
            p[0x34] = SB(6);
            p[0x35] = 1;
            p[0x36] = SB(6);
            MH_CALL(Gfx_CommitPrim)(3, 0x44);
            s = Sc();
            ++j;
        } while (j < s[0xA]);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A9350: the trail pushed back, the screen point (+0x2E, +0x30)
// into point +0xB.
S08_EXPORT void __cdecl EvilEyeBeam_Record(void) {
    const unsigned char* const s = Sc();
    TrailShift(s);
    SetWord(Trail(s[4], s[0xB]), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetWord(Trail(s[4], s[0xB]) + 2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
}

// original 0x4A93E0: the trail pushed back, point +0xB erased (0xFFFF, 0xFFFF).
S08_EXPORT void __cdecl EvilEyeBeam_Erase(void) {
    const unsigned char* const s = Sc();
    TrailShift(s);
    SetWord(Trail(s[4], s[0xB]), 0xFFFF);
    SetWord(Trail(s[4], s[0xB]) + 2, 0xFFFF);
}

// original 0x4A9470: a spark. A call through EvilEyeSpark_Steps (three
// entries) by +2; then while +0 and +2 are set, the spark drawn.
S08_EXPORT void __cdecl EvilEyeSpark_Run(void) {
    CellCall(::bof3::Img(0x65A93C), 3, Sc()[2], "EvilEyeSpark_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::EvilEyeSpark_Draw);
}

// original 0x4A94A0: the screen point, 8 lower; +0x5D..+0x5F each Rand & 3 +
// 1 (the colour weights); +9 0, +0xA 0x10, +2 on.
S08_EXPORT void __cdecl EvilEyeSpark_Start(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    AddWord(Sc() + 0x30, 8);
    for (unsigned k = 0x5D; k < 0x60; ++k) {
        const std::uint32_t r = RandCall();
        Sc()[k] = static_cast<unsigned char>((r & 3) + 1);
    }
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A9510: a Rand whose answer is tested and dropped; the screen
// point up and left by 1; +9 up by 4; at 0x10 +2 on.
S08_EXPORT void __cdecl EvilEyeSpark_Grow(void) {
    RandCall();
    AddWord(Sc() + 0x2E, 0xFFFF);
    AddWord(Sc() + 0x30, 0xFFFF);
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4A9550: the screen point up by 2, +9 up; a Rand dropped as in
// _Grow; left by 1; +0xA down; at 0 the owner's +0xB down and the task freed.
S08_EXPORT void __cdecl EvilEyeSpark_Fade(void) {
    AddWord(Sc() + 0x30, 0xFFFE);
    Inc(Sc()[9]);
    RandCall();
    AddWord(Sc() + 0x2E, 0xFFFF);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A95A0: one semi-transparent textured quad (tpage 0x55, layer
// 2, linked at the task) round the screen point, radius +9 x 2 at the angles
// 0x200, 0x600, 0xE00, 0xA00; texture page (0, 2, 0x340, 0x100), CLUT (0x20,
// 0x1FA), u / v 0x80..0xA0 x 0x20..0x40; the colour +0x5D..+0x5F (signed)
// x +0xA.
S08_EXPORT void __cdecl EvilEyeSpark_Draw(void) {
    DrawMode(0x55);
    LinkAtTask(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetSW(0, Sc()[9] * 2u);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned k = 0; k < 4; ++k) {
        int v = MH_CALL(Math_Cos)(kAngles[k]);
        PutFloat(p + 8 + 0x10 * k, Mul12(v, SS(0)) + S16(Sc() + 0x2E));
        v = MH_CALL(Math_Sin)(kAngles[k]);
        PutFloat(p + 0xC + 0x10 * k, Mul12(v, SS(0)) + S16(Sc() + 0x30));
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0x20, 0x1FA));
    p[0x14] = 0x80;
    p[0x34] = 0x80;
    p[0x15] = 0x20;
    p[0x24] = 0xA0;
    p[0x25] = 0x20;
    p[0x35] = 0x40;
    p[0x44] = 0xA0;
    p[0x45] = 0x40;
    const unsigned char* const s = Sc();
    SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5D]) * static_cast<int>(s[0xA])));
    SetSW(0xC, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * static_cast<int>(s[0xA])));
    const unsigned char red = SB(0xA);
    SetSW(0xE, static_cast<unsigned>(static_cast<signed char>(s[0x5F]) * static_cast<int>(s[0xA])));
    p[4] = red;
    p[5] = SB(0xC);
    p[6] = SB(0xE);
    LinkAtTask(2, 0x48);
}

void EvilEyeBeam_Inject() {
    BOF3_INJECT(EvilEye_Task);
    BOF3_INJECT(EvilEye_Start);
    BOF3_INJECT(EvilEyeChild_Task);
    BOF3_INJECT(EvilEyeBeam_Run);
    BOF3_INJECT(EvilEyeBeam_Start);
    BOF3_INJECT(EvilEyeBeam_Seek);
    BOF3_INJECT(EvilEyeBeam_Trail);
    BOF3_INJECT(EvilEyeBeam_End);
    BOF3_INJECT(EvilEyeBeam_DrawThin);
    BOF3_INJECT(EvilEyeBeam_DrawWide);
    BOF3_INJECT(EvilEyeBeam_Record);
    BOF3_INJECT(EvilEyeBeam_Erase);
    BOF3_INJECT(EvilEyeSpark_Run);
    BOF3_INJECT(EvilEyeSpark_Start);
    BOF3_INJECT(EvilEyeSpark_Grow);
    BOF3_INJECT(EvilEyeSpark_Fade);
    BOF3_INJECT(EvilEyeSpark_Draw);
}
