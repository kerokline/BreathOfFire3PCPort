// Moved here by tools/move_funcs.py from magic_s09.cpp (docs/source-layout-round.md).
#include "game/magic/elem_breath_mote.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// This group's .data (symbols.toml [[data]]).
// kMotePool: now ElemBreath_Motes (symbols.toml). ElemBreath_Motes: 48 records of 0x84 bytes
constexpr unsigned kMoteRecords = 48;

// The phase handlers of other units a table holds (docs/magic_s09.md
// section 3): the engine's below by address; the rest ours now and named in
// the tables (S30's MagicFx_EndWhenChildrenDone, BattleFx_Finish, S24's
// MagicFx_EndWithChildren, BattleFx_FreeTask, S37's Combustion_Wait, S11's
// MagicFx_UncountAndFree, C2's HolocaustBeam_Grow).
constexpr ::bof3::image::CodeVa kScriptUntilDone{bof3::addr::BattleFx_ScriptUntilDone};  // engine: the script ticked, the sprite queued, +2 on at the done flag

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(Long(at)) + v)); }

// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }

// `shl n` then `sar m` on a dword.
int ShlSar(int v, unsigned n, unsigned m) { return static_cast<int>(static_cast<std::uint32_t>(v) << n) >> m; }

// `cdq / sub eax, edx / sar 1`: a signed halving toward zero.
int Half(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) - static_cast<std::uint32_t>(v >> 31)) >> 1; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* PoolRecord(unsigned index) { return Mem(::bof3::Img(ElemBreath_Motes_addr) + index * 0x84u); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// The projections with the arguments the originals push: the depth and a
// flag pointer past the declared ones (cdecl: the caller pops them).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

#define S09_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The task's +8 and position (+0x34 / +0x38 / +0x3C) the owner's.
void FacingAndPositionFromOwner() {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// Gfx_ClutStrip 0x1A00..0x1A0F (row 26's first sixteen words) back from its
// source with the semi-transparency bit, then the first word plain; the
// strip marked dirty first.
void RestoreRow26Head() {
    Gfx_ClutStripDirty = 1;
    for (unsigned k = 0; k < 0x10; ++k)
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
}

// One MATRIX block as the originals lay it out on their stack: the rotation,
// then the translation RotTrans writes at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};

// Camera_Matrix x the rotation of `angles`, translated by RotTrans of `v`,
// set as the GTE's (the two matrix pushes' common tail). The original pushes
// a third argument (a flag pointer) to Gte_RotTrans, which takes two.
void SetTurnedMatrix(const short* v, const short* angles) {
    Matrix m;
    long flag;
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S09_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// The flying sprites' sway: *at up by the low 20 bits of v x 7 (`lea` x 8, sub,
// `shl 0xC`, `sar 0xC`).
void AddSway(unsigned char* at, int v) { AddLong(at, U(ShlSar(Imul(v, 7), 12, 12))); }

}  // namespace

#define S09_EXPORT extern "C" __attribute__((disable_tail_calls))

// The motes' drift, one frame: the sway angle ((+0xB + +9) & 0x3F) << 6 kept
// in word 0x903852; the heading +0xC the angle +0x14 plus sin x 300 >> 12; the
// position along it by 7 x sin / cos (each address taken before its call);
// the screen point; the quad drawn.
static void ElemBreathMoteDrift() {
    {
        const unsigned char* const s = Sc();
        const unsigned angle = (static_cast<unsigned>(static_cast<unsigned char>(s[0xB] + s[9])) & 0x3F) << 6;
        SetSW(2, angle);
        const int v = Sin(static_cast<short>(angle));
        unsigned char* const t = Sc();
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->step_x = static_cast<std::int32_t>(U(Imul(v, 300) >> 12) + U(tt->step_y));
    }
    {
        unsigned char* const p = Sc() + 0x34;
        AddSway(p, Sin(rv::Current<records::BattleTask>()->step_x));
    }
    {
        unsigned char* const p = Sc() + 0x38;
        AddSway(p, Cos(rv::Current<records::BattleTask>()->step_x));
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::ElemBreathMote_Draw);
}

// original 0x4A9FE0: the kind-2 task. A two-entry stack table by +1 -
// ElemBreath_Start, BattleFx_Finish - then the mote pool walked: every record
// with bit 0 becomes Sprite_Current, its +0x80 the owner, for
// ElemBreathMote_Run; both put back after each (as read after the phase
// call).
S09_EXPORT void __cdecl ElemBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::ElemBreath_Start, bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "ElemBreath_Task");
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kMoteRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(bof3::addr::ElemBreathMote_Run);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// original 0x4AA060: the pool's records +0..+2 cleared; the owner's direction
// and position; the emitter (kind 1, 0x32, +1 0, +0x80 this task); in an event
// battle whose current enemy (0x939AD8) has +0x100 0x29 a second child (+1 1);
// row 26's first sixteen CLUT words back with their STP bits; sound 0x100;
// +0xB 1, +1 on.
S09_EXPORT void __cdecl ElemBreath_Start(void) {
    for (unsigned i = 0; i < kMoteRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    FacingAndPositionFromOwner();
    {
        const unsigned slot = NewTask(0x32);
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(Sc()));
        child[1] = 0;
    }
    if (Mem(::bof3::Img(EventBattle_Id_addr))[0] != 0 && Pointer(::bof3::Img(BattleE3_CurrentEnemy_addr))[0x100] == 0x29) {
        const unsigned slot = NewTask(0x32);
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(Sc()));
        child[1] = 1;
    }
    RestoreRow26Head();
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 1;
    Inc(Sc()[1]);
}

// original 0x4AA1A0: the children's kind-1 task, a jmp through
// ElemBreathChild_Kinds (two entries) by +1, unchecked.
S09_EXPORT void __cdecl ElemBreathChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::ElemBreathEmitter_Task, bof3::addr::ElemBreathEnemy_Task};
    Dispatch(kKinds, 2, Sc()[1], "ElemBreathChild_Task");
}

// original 0x4AA1C0: the emitter. A three-entry stack table by +2:
// ElemBreathEmitter_Start, _Emit, MAGIC104's MagicFx_EndWithChildren.
S09_EXPORT void __cdecl ElemBreathEmitter_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ElemBreathEmitter_Start, bof3::addr::ElemBreathEmitter_Emit,
                                                bof3::addr::MagicFx_EndWithChildren};
    Dispatch(kSteps, 3, Sc()[2], "ElemBreathEmitter_Task");
}

// original 0x4AA1F0: the owner's direction and position; +0xB 0, +9 0, +2 on.
S09_EXPORT void __cdecl ElemBreathEmitter_Start(void) {
    FacingAndPositionFromOwner();
    unsigned char* const s = Sc();
    s[0xB] = 0;
    s[9] = 0;
    Inc(s[2]);
}

// original 0x4AA260: on odd frames a mote from the pool (its "none free" 0xFF
// unchecked, as in the original: record 255 lies at 0x6861BC, past the pool):
// +0x80 this task, +0xB our +9 (its phase along the sway), +0xA its life (8,
// shortening by one each four frames past +9 0x64); our count +0xB up. Every
// frame +9 up; past 0x80 +2 on.
S09_EXPORT void __cdecl ElemBreathEmitter_Emit(void) {
    if (Frame_Counter & 1) {
        const unsigned index = MH_AT(ByteFn, bof3::addr::ElemBreathMote_Alloc)() & 0xFFu;
        unsigned char* const s = Sc();
        unsigned char* const rec = PoolRecord(index);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Key(s));
        rec[0xB] = s[9];
        const unsigned char age = s[9];
        rec[0xA] = age < 0x64 ? static_cast<unsigned char>(8) : static_cast<unsigned char>(8 - ((age - 0x64) >> 2));
        Inc(s[0xB]);
    }
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] > 0x80) Inc(s[2]);
}

// original 0x4AA2F0: the second child. A three-entry stack table by +2:
// ElemBreathEnemy_Start, the engine's 0x43EC10 (the script until its done
// flag), BattleFx_FreeTask.
S09_EXPORT void __cdecl ElemBreathEnemy_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ElemBreathEnemy_Start, kScriptUntilDone.v,
                                                bof3::addr::BattleFx_FreeTask};
    Dispatch(kSteps, 3, Sc()[2], "ElemBreathEnemy_Task");
}

// original 0x4AA320: Sprite_Current made the owner (the breath task; not put
// back); its +0xB and +9 0; animation 2; its +1 and +2 1.
S09_EXPORT void __cdecl ElemBreathEnemy_Start(void) {
    unsigned char* const o = Owner();
    Sprite_Current = o;
    o[0xB] = 0;
    Sc()[9] = 0;
    MH_CALL(Sprite_SetAnimation)(2);
    Sc()[1] = 1;
    Sc()[2] = 1;
}

// original 0x4AA360: a mote (Sprite_Current a pool record). A three-entry
// stack table by +2 - ElemBreathMote_Start, _Flow, _Fade -; then while +0 is
// set its matrix, its flare, the matrix popped.
S09_EXPORT void __cdecl ElemBreathMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ElemBreathMote_Start, bof3::addr::ElemBreathMote_Flow,
                                                bof3::addr::ElemBreathMote_Fade};
    Dispatch(kSteps, 3, Sc()[2], "ElemBreathMote_Run");
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::ElemBreathMote_PushMatrix);
    Call0(bof3::addr::ElemBreathMote_DrawFlare);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AA3B0: the task to the side's centre; its angle from the owner
// (Math_Ratan2 of the offset); the owner's direction; an offset by the acting
// enemy's byte +0x8C (unchecked: a party actor reads below the enemy records)
// - 0x10 and 0x72 farther and higher - turned by the direction onto the
// owner's position; +0x14 the angle and 0xFFF, +9 0, +2 on.
S09_EXPORT void __cdecl ElemBreathMote_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    int angle;
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const unsigned char* const o = Owner();
        const std::int32_t dz = static_cast<std::int32_t>(U(ts->z) - U(rv::As<records::SpriteHead>(o)->z));
        const std::int32_t dx = static_cast<std::int32_t>(U(ts->x) - U(rv::As<records::SpriteHead>(o)->x));
        angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    }
    Sc()[8] = Owner()[8];
    {
        const unsigned kind = EnemyRecord(Mem(::bof3::Img(at::kActor))[0])[0x8C];
        std::int32_t reach, lift;
        if (kind == 0x10) {
            reach = 0x10000;
            lift = 0x800000;
        } else if (kind == 0x72) {
            reach = 0x1C000;
            lift = 0x1000000;
        } else {
            reach = 0x6000;
            lift = 0x400000;
        }
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_x = reach;
        ts->step_z = 0;
        rv::Store(ts->step_y, lift);
    }
    Turn(Sc());
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    const unsigned char* const o = Owner();
    ts->x = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(o)->x) + U(ts->step_x));
    ts->z = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(o)->z) + U(ts->step_z));
    rv::Store(ts->height, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(o)->height) + U(ts->step_y)));
    rv::Store(ts->step_y, static_cast<std::int32_t>(U(angle) & 0xFFF));
    s[9] = 0;
    Inc(s[2]);
}

// original 0x4AA510: the drift; +9 up, at 0x10 +2 on.
S09_EXPORT void __cdecl ElemBreathMote_Flow(void) {
    ElemBreathMoteDrift();
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] == 0x10) Inc(s[2]);
}

// original 0x4AA5D0: the drift; +9 up, the life +0xA down; at 0 the emitter's
// count +0xB down and the record freed (+0..+4 0).
S09_EXPORT void __cdecl ElemBreathMote_Fade(void) {
    ElemBreathMoteDrift();
    unsigned char* const s = Sc();
    Inc(s[9]);
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x4AA6D0: a draw-mode packet (tpage 0x35) linked at the task; a
// semi-transparent textured gouraud quad (tpage 0x340 / 0x100 at 1, CLUT row
// 0x1FA) round the screen point: radius word 0x903850 ((+9 + 1) x 6 below 8,
// then +9 x 2 + 0x20), centre words 0x903854 / 0x903856, shade 0x903858 +0xA
// x 20; the corners at 0xC00, 0x800, 0 and 0x400 (sin x, cos y; the scratch
// read again after every call) as floats; linked (0x54).
S09_EXPORT void __cdecl ElemBreathMote_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    {
        const unsigned char* const s = Sc();
        const unsigned age = s[9];
        SetSW(0, age < 8 ? (age + 1) * 6 : age * 2 + 0x20);
        SetSW(8, s[0xA] * 20u);
        SetSW(4, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(6, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    }
    static constexpr struct { int angle; unsigned x, y; } kCorners[4] = {
        {0xC00, 8, 0xC}, {0x800, 0x1C, 0x20}, {0, 0x30, 0x34}, {0x400, 0x44, 0x48}};
    for (const auto& c : kCorners) {
        int v = Sin(c.angle);
        PutFloat(p + c.x, Mul12(v, SS(0)) + SS(4));
        v = Cos(c.angle);
        PutFloat(p + c.y, Mul12(v, SS(0)) + SS(6));
    }
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, tpage & 0xFFFF);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, clut & 0xFFFF);
    p[0x14] = 1;
    p[0x15] = 1;
    p[0x28] = 0x1F;
    p[0x29] = 1;
    p[0x3C] = 1;
    p[0x3D] = 0x1F;
    p[0x50] = 0x1F;
    p[0x51] = 0x1F;
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au, 0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(8);
    LinkAtSprite(0x54);
}

// original 0x4AA980: the mote's matrix pushed: its x / z, the owner's height,
// no rotation.
S09_EXPORT void __cdecl ElemBreathMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    short v[4];
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    v[0] = static_cast<short>((ts->x >> 9) - 0x4000);
    v[1] = static_cast<short>((ts->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-Half(S16(Owner() + 0x3E)));
    v[3] = 0;
    SetTurnedMatrix(v, angles);
}

// original 0x4AAA30: a draw-mode packet (tpage 0x55) committed to layer 5;
// radius word 0x903850 (+9 + 6) x 10, shade 0x903858 +0xA x 6; eight
// semi-transparent gouraud triangles fanned from the origin (Prim_VertexScratch:
// the origin, the last edge, the next at the angle, the scratch read again
// after every call), projected (Gte_RotTransPers3, Gte_PrimDepths3_10B) and
// committed to layer 5; a closing draw-mode packet (tpage 0x15).
S09_EXPORT void __cdecl ElemBreathMote_DrawFlare(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, (s[9] + 6u) * 10u);
        SetSW(8, s[0xA] * 6u);
    }
    int v = Sin(0);
    SetVW(0x10, U(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x12, U(Mul12(v, SS(0))));
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t lx = VW(0x10), ly = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, lx);
        SetVW(0xA, ly);
        v = Sin(a);
        SetVW(0x10, U(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x12, U(Mul12(v, SS(0))));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        long depth, flag;
        S09_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), p + 8, p + 0x18, p + 0x28, &depth, &flag);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(8);
        p[5] = SB(8);
        p[6] = SB(8);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4AABE0: the mote pool's alloc: the first of its 48 records with
// bit 0 clear gets it, its index in al; 0xFF when all are taken.
S09_EXPORT unsigned char __cdecl ElemBreathMote_Alloc(void) {
    for (unsigned i = 0; i < kMoteRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (rec[0] & 1) continue;
        rec[0] |= 1;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void ElemBreathMote_Inject() {
    BOF3_INJECT(ElemBreath_Task);
    BOF3_INJECT(ElemBreath_Start);
    BOF3_INJECT(ElemBreathChild_Task);
    BOF3_INJECT(ElemBreathEmitter_Task);
    BOF3_INJECT(ElemBreathEmitter_Start);
    BOF3_INJECT(ElemBreathEmitter_Emit);
    BOF3_INJECT(ElemBreathEnemy_Task);
    BOF3_INJECT(ElemBreathEnemy_Start);
    BOF3_INJECT(ElemBreathMote_Run);
    BOF3_INJECT(ElemBreathMote_Start);
    BOF3_INJECT(ElemBreathMote_Flow);
    BOF3_INJECT(ElemBreathMote_Fade);
    BOF3_INJECT(ElemBreathMote_Draw);
    BOF3_INJECT(ElemBreathMote_PushMatrix);
    BOF3_INJECT(ElemBreathMote_DrawFlare);
    BOF3_INJECT(ElemBreathMote_Alloc);
}
