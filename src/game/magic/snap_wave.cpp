// Moved here by tools/move_funcs.py from magic_s04.cpp (docs/source-layout-round.md).
#include "game/magic/snap_wave.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
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
// sixteen bytes (0x903850..0x90385F) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF), read again after every call as the
// originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The cells the overlays read beyond the harness's names.
// kActorSprite: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kPoolStride = 0x84;

constexpr unsigned kPoolRecords = 64;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddL(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

void AddW(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }

void CopyL(unsigned char* to, const unsigned char* from) { SetLong(to, Long(from)); }

std::uint32_t Addr(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl l` then `sar r` on a dword.
int ShlSar(int v, unsigned l, unsigned r) { return static_cast<int>(static_cast<std::uint32_t>(v) << l) >> r; }

// `cdq / sub eax, edx / sar 1`: a signed halving toward zero.
int Half(int v) { return (v - (v >> 31)) >> 1; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* PoolRecord(unsigned i) { return Mem(Addr(SnapSpark_Pool) + i * kPoolStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions called directly, and the other units' phases a
// stack table holds, by address, as the originals call them: in the game
// Capcom's code or the jmp Inject put there, in the fuzz that address's
// recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's code this group calls (docs/magic_s04.md section 3); other groups'
// phase handlers (S35's LastResort_WaitChildren, S05's Magic018Row53_Wait,
// S11's MagicFx_UncountAndFree) are ours now and named in the tables.
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};       // engine: +0xC / +0x10 of a task turned by its +8

constexpr ::bof3::image::CodeVa kPolyF3{bof3::addr::Gpu_SetPolyF3};           // libgpu SetPolyF3 by shape (code 0x20), unnamed

using TaskFn = void (__cdecl*)(unsigned char*);

using PrimFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call or jmp, `[table + phase * 4]`, read in place (the fuzz
// swaps the cells for recorders); the table's own entries only - what follows
// is the next table or data.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The GTE callees with the arguments the originals push: Gte_RotTrans a flag
// pointer more than it takes, the projections a depth and a flag pointer.
using RotTransFn = void (__cdecl*)(const short*, long*, long*);

using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S04_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Vertices 0..2 of Prim_VertexScratch projected to prim + 8, + 8 + step,
// + 8 + 2 step; vertices 0..3 to four.
void Rtp3(unsigned char* prim, unsigned step) {
    long p, flag;
    S04_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 8 + step, prim + 8 + 2 * step, &p, &flag);
}

void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S04_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// The matrix pushes' common tail (MagicFx_PushActorMatrix's shape): the
// translation RotTrans of `v`, the rotation of `angles`, Camera_Matrix x it,
// both set. One MATRIX block as the original lays it out on its stack,
// RotTrans writing its translation at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};

void SetMatrix(const short* angles, const short* v) {
    Matrix m;
    long flag;
    S04_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// ((+0x34 sar 9) - 0x4000, (+0x38 sar 9) - 0x4000, height): the task's point in
// the GTE's units.
void GroundPoint(const unsigned char* s, short height, short* v) {
    v[0] = static_cast<short>((rv::As<records::SpriteHead>(s)->x >> 9) - 0x4000);
    v[1] = static_cast<short>((rv::As<records::SpriteHead>(s)->z >> 9) - 0x4000);
    v[2] = height;
    v[3] = 0;
}

}  // namespace

#define S04_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// Every live record of SnapSpark_Pool run through SnapSpark_Task as
// Sprite_Current, its +0x80 the owner cell; both put back after each. The task
// and the owner are read after the phase call that precedes the walk.
void WalkPool() {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(bof3::addr::SnapSpark_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// SnapWave_DrawRingA / B: sixteen semi-transparent gouraud quads round a
// ring whose radius swells with a sine, each from the previous rim point to
// this one, its second edge `dz` away (z - 0x50 or z + 0x50); shade +4 x 12 at
// the far edge, 1 at the near. The previous point lives in vertex 3
// (0x9037B8..0x9037BD) from one quad to the next.
void DrawRing(int dz) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
    SetSD(8, Sc()[4] * 12u);
    {
        const int r = Sin(0);
        const unsigned char* const s = Sc();
        SetSD(0, static_cast<std::uint32_t>(ShlSar(r, 6, 12)));
        const unsigned a = static_cast<unsigned>(Mem(Addr(SnapWave_FacingPhase) + s[8])[0]) << 7;
        SetSD(4, a);
        SetVW(0x18, static_cast<unsigned>(ShlSar(Sin(static_cast<int>(a)), 7, 12)));
        SetVW(0x1A, static_cast<unsigned>(ShlSar(Cos(SD(4)), 7, 12)));
        const int z = Sin(0);
        SetVW(0x1C, static_cast<unsigned>(Mul12(z, SD(0))));
    }
    for (unsigned i = 1, angle = 0x80; angle < 0x880; ++i, angle += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const int r = Sin(static_cast<int>(angle));
            const unsigned char* const s = Sc();
            SetSD(0, static_cast<std::uint32_t>(ShlSar(r, 6, 12)));
            const unsigned phase = Mem(Addr(SnapWave_FacingPhase) + s[8])[0];
            const std::uint16_t x = VW(0x18), y = VW(0x1A), z = VW(0x1C);
            SetVW(2, y);
            SetVW(0, x);
            SetVW(4, static_cast<unsigned>(z + dz));
            SetVW(0x10, x);
            SetVW(0x12, y);
            SetVW(0x14, z);
            const unsigned a = ((phase + i) & 0x1F) << 7;
            SetSD(4, a);
        }
        SetVW(0x18, static_cast<unsigned>(ShlSar(Sin(SD(4)), 7, 12)));
        SetVW(0x1A, static_cast<unsigned>(ShlSar(Cos(SD(4)), 7, 12)));
        {
            const int r = Sin(static_cast<int>((i & 3) << 10));
            SetVW(0x1C, static_cast<unsigned>(Mul12(r, SD(0))));
            SetVW(8, VW(0x18));
            SetVW(0xA, VW(0x1A));
            SetVW(0xC, static_cast<unsigned>(VW(0x1C) + dz));
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(8);
        LinkAtSprite(0x44);
    }
}

}  // namespace

// original 0x49E9F0: the kind-2 task. A four-entry stack table by +1:
// Snap_Start, MAGIC167's 0x4EF7C0, Snap_Buff, MagicFx_EndWhenChildrenDone;
// then the spark pool walked.
S04_EXPORT void __cdecl Snap_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Snap_Start, bof3::addr::LastResort_WaitChildren,
                                                 bof3::addr::Snap_Buff, bof3::addr::MagicFx_EndWhenChildrenDone};
    StackCall(kPhases, 4, Sc()[1], "Snap_Task");
    WalkPool();
}

// original 0x49EA80: the spark pool's +0..+2 cleared; the task on the source
// sprite; row 26's first two CLUTs back from their source (no STP bit); the
// wave (kind 1, 0x29) with +0x80 this task; +0xB 2, +1 on.
S04_EXPORT void __cdecl Snap_Start(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    const unsigned char* const src = Source();
    Sc()[8] = src[8];
    CopyL(Sc() + 0x34, src + 0x34);
    CopyL(Sc() + 0x38, src + 0x38);
    CopyL(Sc() + 0x3C, src + 0x3C);
    for (unsigned k = 0x1A00; k < 0x1A10; ++k) {
        Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
        Gfx_ClutStrip[k + 0x10] = Gfx_ClutStripSource[k + 0x10];
    }
    Gfx_ClutStripDirty = 1;
    const unsigned slot = NewTask(0x29);
    unsigned char* const s = Sc();
    SetLong(TaskSlot(slot) + 0x80, static_cast<std::int32_t>(Addr(s)));
    s[0xB] = 2;
    Inc(Sc()[1]);
}

// original 0x49EB40: when the target is not out and MagicFx_ApplyBuff(1,
// target) takes, a child (kind 1, 0x48) with +4 4, +9 0x18, +0xA 4 and this
// task's +0xB up; +1 on.
S04_EXPORT void __cdecl Snap_Buff(void) {
    if (MH_CALL(Battle_ActorIsOut)(TargetByte()) == 0 && MH_CALL(MagicFx_ApplyBuff)(1, TargetByte()) != 0) {
        const unsigned slot = NewTask(0x48);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Addr(s));
        child[4] = 4;
        child[9] = 0x18;
        child[0xA] = 4;
        Inc(s[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x49EBC0: the wave's kind-1 task, a jmp through SnapWave_TaskTable
// (one entry) by +1.
S04_EXPORT void __cdecl SnapWave_Task(void) { CellCall(Addr(SnapWave_TaskTable), 1, Sc()[1], "SnapWave_Task"); }

// original 0x49EBE0: a call through SnapWave_Steps (five entries) by +2; then
// while +0 is set the two rings, each under its own matrix.
S04_EXPORT void __cdecl SnapWave_Run(void) {
    CellCall(Addr(SnapWave_Steps), 5, Sc()[2], "SnapWave_Run");
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::SnapWave_PushMatrixA);
    Call0(bof3::addr::SnapWave_DrawRingA);
    MH_CALL(Gte_PopMatrix)();
    Call0(bof3::addr::SnapWave_PushMatrixB);
    Call0(bof3::addr::SnapWave_DrawRingB);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x49EC20: the owner's direction; the offset (0x20000, 0) turned,
// from the owner's point, 0x1000000 above it; the step (-0x6000, 0) turned; +4
// and +0xB 0, +9 0x10, +0xA 4 (the swings), +2 on.
S04_EXPORT void __cdecl SnapWave_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = 0x20000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x) +
                                                   static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z) +
                                                   static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_z)));
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + 0x1000000u));
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(0xFFFFA000u));
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x49ECF0: the point on by the step, +9 down by 4; at 0 up to 32
// sparks from the pool (each +0x80 this task, +0 or 0x41, +0xB its number, +9
// Rand & 7 + 1; +0xB up for each), the target flagged 0x10 on the first swing
// (+0xA 4), the step (0x3000, 0) turned, sound 0x100, +2 on.
S04_EXPORT void __cdecl SnapWave_Burst(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddL(s + 0x34, static_cast<std::uint32_t>(ts->step_x));
        AddL(s + 0x38, static_cast<std::uint32_t>(ts->step_z));
        AddB(s[9], 0xFC);
    }
    if (Sc()[9] != 0) return;
    for (unsigned i = 0; i < 0x20; ++i) {
        const unsigned k = MH_AT(ByteFn, bof3::addr::SnapSpark_Alloc)() & 0xFFu;
        if (k == 0xFF) continue;
        unsigned char* const rec = PoolRecord(k);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Addr(Sc()));
        rec[0] = static_cast<unsigned char>(rec[0] | 0x41);
        rec[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        rec[9] = static_cast<unsigned char>((r & 7) + 1);
        Inc(Sc()[0xB]);
    }
    if (Sc()[0xA] == 4) MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    rv::Current<records::BattleTask>()->step_x = 0x3000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    MH_CALL(Sound_PlayById)(0x100);
    Inc(Sc()[2]);
}

// original 0x49EDF0: the point on by the step, +9 up by 2; at 0x10 (0xC on the
// last swing) +0xA down: at 0 the target flagged 0x40, sound 0x201, the
// owner's +0xB down, +2 on; else the step (-0x6000, 0) turned and +2 back to
// SnapWave_Burst.
S04_EXPORT void __cdecl SnapWave_Swing(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddL(s + 0x34, static_cast<std::uint32_t>(ts->step_x));
        AddL(s + 0x38, static_cast<std::uint32_t>(ts->step_z));
    }
    unsigned char* s = Sc();
    const unsigned limit = s[0xA] == 1 ? 0xCu : 0x10u;
    AddB(s[9], 2);
    s = Sc();
    if (s[9] != limit) return;
    Dec(s[0xA]);
    s = Sc();
    if (s[0xA] == 0) {
        MH_CALL(Battle_SetTargetFlag40)(TargetByte());
        MH_CALL(Sound_PlayById)(0x201);
        Dec(Owner()[0xB]);
        Inc(Sc()[2]);
        return;
    }
    rv::Store(rv::As<records::BattleTask>(s)->step_x, static_cast<std::int32_t>(0xFFFFA000u));
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    Dec(Sc()[2]);
}

// original 0x49EEB0: once the sparks are gone (+0xB 0), +4 down; at 0 the
// owner's +0xB down and the task freed.
S04_EXPORT void __cdecl SnapWave_End(void) {
    unsigned char* const s = Sc();
    if (s[0xB] != 0) return;
    Dec(s[4]);
    if (Sc()[4] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49EEE0: the lower ring's matrix: tipped by +9 about x or y by the
// facing +8 (a four-entry jump table), at the task's point, (+0x3E + 0x20) / 2
// up.
S04_EXPORT void __cdecl SnapWave_PushMatrixA(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    const int b = s[9];
    short angles[4] = {0, 0, 0, 0};
    switch (facing) {
    case 0: angles[1] = static_cast<short>((0x40 - b) << 6); break;
    case 1: angles[0] = static_cast<short>(b << 6); break;
    case 2: angles[1] = static_cast<short>(b << 6); break;
    case 3: angles[0] = static_cast<short>((0x40 - b) << 6); break;
    default: bof3::Fatal("SnapWave_PushMatrixA: facing %u past the four-entry jump table", facing);
    }
    short v[4];
    GroundPoint(s, static_cast<short>(-Half(S16(s + 0x3E) + 0x20)), v);
    SetMatrix(angles, v);
}

// original 0x49F000: the upper ring's matrix: tipped the other way (by 0x40 -
// +9 where A tips by +9), (+0x3E) / 2 up.
S04_EXPORT void __cdecl SnapWave_PushMatrixB(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    const int b = s[9];
    short angles[4] = {0, 0, 0, 0};
    switch (facing) {
    case 0: angles[1] = static_cast<short>(b << 6); break;
    case 1: angles[0] = static_cast<short>((0x40 - b) << 6); break;
    case 2: angles[1] = static_cast<short>((0x40 - b) << 6); break;
    case 3: angles[0] = static_cast<short>(b << 6); break;
    default: bof3::Fatal("SnapWave_PushMatrixB: facing %u past the four-entry jump table", facing);
    }
    short v[4];
    GroundPoint(s, static_cast<short>(-Half(S16(s + 0x3E))), v);
    SetMatrix(angles, v);
}

// original 0x49F120: the lower ring (the second edge 0x50 below).
S04_EXPORT void __cdecl SnapWave_DrawRingA(void) { DrawRing(-0x50); }

// original 0x49F3A0: the upper ring (the second edge 0x50 above).
S04_EXPORT void __cdecl SnapWave_DrawRingB(void) { DrawRing(0x50); }

// original 0x49F620: a spark record's task, a jmp through SnapSpark_TaskTable
// (one entry) by +1.
S04_EXPORT void __cdecl SnapSpark_Task(void) { CellCall(Addr(SnapSpark_TaskTable), 1, Sc()[1], "SnapSpark_Task"); }

// original 0x49F640: a call through SnapSpark_Steps (two entries) by +2; then
// while +0 and +9 are set the spark under its matrix.
S04_EXPORT void __cdecl SnapSpark_Run(void) {
    CellCall(Addr(SnapSpark_Steps), 2, Sc()[2], "SnapSpark_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[9] == 0) return;
    Call0(bof3::addr::SnapSpark_PushMatrix);
    Call0(bof3::addr::SnapSpark_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x49F680: +9 down; at 0 the spark starts on the source sprite,
// 0x80 out along its angle +0xB x 0x80; +0x14 (the rise) Rand & 7 + 0x80, +0x20
// (its fall) -12, +9 0x10, +2 on.
S04_EXPORT void __cdecl SnapSpark_Launch(void) {
    Dec(Sc()[9]);
    const unsigned char* const s = Sc();
    if (s[9] != 0) return;
    const unsigned a = static_cast<unsigned>(s[0xB]) << 7;
    const unsigned char* const src = Source();
    SetSD(4, a);
    {
        const int r = Sin(static_cast<int>(a));
        rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(r, 8, 9)) +
                                                       static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->x)));
    }
    {
        const int r = Cos(SD(4));
        rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(r, 8, 9)) +
                                                       static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->z)));
    }
    CopyL(Sc() + 0x3C, src + 0x3C);
    const std::uint32_t r = RandCall();
    rv::Store(rv::Current<records::BattleTask>()->step_y, static_cast<std::int32_t>((r & 7) + 0x80));
    rv::Store(rv::Current<records::BattleTask>()->work_20, -12);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x49F730: out along the angle by 0x200 a frame (x through the task
// read before Math_Sin, z before Math_Cos), up by +0x14 which falls by +0x20;
// +9 down, at 0 the owner's +0xB down and +0..+4 cleared (the record free).
S04_EXPORT void __cdecl SnapSpark_Fly(void) {
    {
        unsigned char* const s = Sc();
        const unsigned a = static_cast<unsigned>(s[0xB]) << 7;
        unsigned char* const x = s + 0x34;
        SetSD(4, a);
        const int r = Sin(static_cast<int>(a));
        AddL(x, static_cast<std::uint32_t>(ShlSar(r, 10, 9)));
    }
    {
        unsigned char* const z = Sc() + 0x38;
        const int r = Cos(SD(4));
        AddL(z, static_cast<std::uint32_t>(ShlSar(r, 10, 9)));
    }
    {
        unsigned char* const s = Sc();
        AddL(s + 0x14, static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20));
    }
    {
        unsigned char* const s = Sc();
        AddW(s + 0x3E, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
    }
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x49F7F0: the spark's matrix, turned about z by +9 x 0x40, at its
// point, +0x3E / 2 up.
S04_EXPORT void __cdecl SnapSpark_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const short angles[4] = {0, 0, static_cast<short>(s[9] << 6), 0};
    short v[4];
    GroundPoint(s, static_cast<short>(-Half(S16(s + 0x3E))), v);
    SetMatrix(angles, v);
}

// original 0x49F8A0: one semi-transparent flat triangle of radius 16 (corners
// at 0, 0x555, 0xAAA), its colour three (Rand & 0xF) x +9 bytes; tpage 0x35;
// both linked at the task.
S04_EXPORT void __cdecl SnapSpark_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_AT(PrimFn, kPolyF3.v)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetVW(0, static_cast<unsigned>(ShlSar(Sin(0), 4, 12)));
    SetVW(2, static_cast<unsigned>(ShlSar(Cos(0), 4, 12)));
    SetVW(4, 0);
    SetVW(8, static_cast<unsigned>(ShlSar(Sin(0x555), 4, 12)));
    {
        const int r = Cos(0x555);
        SetVW(0xC, 0);
        SetVW(0xA, static_cast<unsigned>(ShlSar(r, 4, 12)));
    }
    SetVW(0x10, static_cast<unsigned>(ShlSar(Sin(0xAAA), 4, 12)));
    SetVW(0x12, static_cast<unsigned>(ShlSar(Cos(0xAAA), 4, 12)));
    SetVW(0x14, 0);
    Rtp3(p, 0xC);
    MH_CALL(Gte_PrimDepths3_0C)(p);
    for (unsigned k = 4; k < 7; ++k) {
        const std::uint32_t r = RandCall();
        p[k] = static_cast<unsigned char>((r & 0xF) * Sc()[9]);
    }
    LinkAtSprite(0x2C);
}

// original 0x49FA10: the first free record of SnapSpark_Pool (bit 0 clear)
// taken (bit 0 set) and its index answered in al; 0xFF when all 64 are live.
S04_EXPORT unsigned char __cdecl SnapSpark_Alloc(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) == 0) {
            rec[0] = static_cast<unsigned char>(rec[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

void SnapWave_Inject() {
    BOF3_INJECT(Snap_Task);
    BOF3_INJECT(Snap_Start);
    BOF3_INJECT(Snap_Buff);
    BOF3_INJECT(SnapWave_Task);
    BOF3_INJECT(SnapWave_Run);
    BOF3_INJECT(SnapWave_Start);
    BOF3_INJECT(SnapWave_Burst);
    BOF3_INJECT(SnapWave_Swing);
    BOF3_INJECT(SnapWave_End);
    BOF3_INJECT(SnapWave_PushMatrixA);
    BOF3_INJECT(SnapWave_PushMatrixB);
    BOF3_INJECT(SnapWave_DrawRingA);
    BOF3_INJECT(SnapWave_DrawRingB);
    BOF3_INJECT(SnapSpark_Task);
    BOF3_INJECT(SnapSpark_Run);
    BOF3_INJECT(SnapSpark_Launch);
    BOF3_INJECT(SnapSpark_Fly);
    BOF3_INJECT(SnapSpark_PushMatrix);
    BOF3_INJECT(SnapSpark_Draw);
    BOF3_INJECT(SnapSpark_Alloc);
}
