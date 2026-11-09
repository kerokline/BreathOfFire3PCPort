// Moved here by tools/move_funcs.py from magic_s37.cpp (docs/source-layout-round.md).
#include "game/magic/magma_breath_record.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s37.h"
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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

constexpr auto& LongMagicS37 = move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

// The two pools of task-like records (0x84 bytes, like the task slots) the
// overlays walk from their kind-2 tasks, and MAGIC219's spawn delays (24
// bytes, read in place).
// kMagmaPool: now MagmaBreath_Pool (symbols.toml). MagmaBreath_Pool, 32 records
constexpr unsigned kMagmaRecords = 32;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::int32_t SD(unsigned k) { return LongMagicS37(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

std::uint32_t Key(const void* p) { return Low32(p); }

void CopyLong(unsigned char* to, const unsigned char* from) { SetLong(to, LongMagicS37(from)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }

unsigned char* MagmaRecord(unsigned index) { return Mem(::bof3::Img(MagmaBreath_Pool_addr) + index * 0x84u); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

void Sound(unsigned id) { MH_CALL(Sound_PlayById)(static_cast<unsigned short>(id)); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

using TaskFn = void (__cdecl*)(unsigned char*);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// A kind-2 task's walk of its pool (after its phase): every record with bit 0
// of +0 is run through `task` as Sprite_Current, its +0x80 the owner; the
// task's own two cells, read once after the phase, put back after each.
void WalkPool(std::uint32_t pool, unsigned records, std::uint32_t task) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = LongMagicS37(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = LongMagicS37(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// The first of a pool's records without bit 0 of +0 gets it; its index, 0xFF
// when all are taken.
unsigned char PoolAlloc(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// A pool's bytes +0..+2 of every record cleared.
void ClearPool(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// The callees with the arguments the originals push: both projections get a
// flag pointer past the depth, which ours does not read.
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

#define S37_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Prim_VertexScratch's first three SVECTORs projected to +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// One MATRIX block as the originals lay it out on their stack: the rotation,
// then the translation RotTrans writes at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F59D0: the kind-2 task. A four-entry stack table by +1
// (MagmaBreath_Start, _Spawn, _Strike, BattleFx_Finish), unchecked; then the
// pool walked through MagmaBreathRecord_Task.
S37_EXPORT void __cdecl MagmaBreath_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::MagmaBreath_Start, addr::MagmaBreath_Spawn,
                                                 addr::MagmaBreath_Strike, addr::BattleFx_Finish};
    Dispatch(kPhases, 4, Sc()[1], "MagmaBreath_Task");
    WalkPool(::bof3::Img(MagmaBreath_Pool_addr), kMagmaRecords, addr::MagmaBreathRecord_Task);
}

// original 0x4F5A60: the pool's +0..+2 cleared; the task at the side's
// centre; +0xB 0, +1 on; CLUT rows 2 and 26 back whole with their STP bits,
// the first word of each without; sound 0x100.
S37_EXPORT void __cdecl MagmaBreath_Start(void) {
    ClearPool(::bof3::Img(MagmaBreath_Pool_addr), kMagmaRecords);
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned k = 0; k < 0x100; ++k) {
        Gfx_ClutStrip[0x200 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x200 + k] | 0x8000);
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    }
    {
        const unsigned short first2 = Gfx_ClutStripSource[0x200], first26 = Gfx_ClutStripSource[0x1A00];
        Gfx_ClutStrip[0x200] = first2;
        Gfx_ClutStrip[0x1A00] = first26;
    }
    Gfx_ClutStripDirty = 1;
    Sound(0x100);
}

// original 0x4F5B00: 24 pool records (MagmaBreath_PoolAlloc; a full pool
// skips one): +0x80 this task, +1 0, +0xB the number, +9 MagmaBreath_SpawnDelays
// [number] + (Rand & 1) + 1, this task's +0xB up; then +9 8, +1 on.
S37_EXPORT void __cdecl MagmaBreath_Spawn(void) {
    for (unsigned i = 0; i < 24; ++i) {
        const unsigned index = MH_AT(ByteFn, addr::MagmaBreath_PoolAlloc)() & 0xFFu;
        if (index == 0xFF) continue;
        unsigned char* const rec = MagmaRecord(index);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Key(Sc()));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        rec[9] = static_cast<unsigned char>((r & 1) + Mem(::bof3::Img(MagmaBreath_SpawnDelays_addr))[i] + 1);
        Inc(Sc()[0xB]);
    }
    Sc()[9] = 8;
    Inc(Sc()[1]);
}

// original 0x4F5B80: +9 down; at 0 sound 0x101, the target flagged 0x10, +1
// on.
S37_EXPORT void __cdecl MagmaBreath_Strike(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sound(0x101);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[1]);
}

// original 0x4F5BC0: a record's task, a jmp through MagmaBreathRecord_TaskTable
// (one entry) by +1, unchecked.
S37_EXPORT void __cdecl MagmaBreathRecord_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::MagmaBreathRecord_Run};
    Dispatch(kKinds, 1, Sc()[1], "MagmaBreathRecord_Task");
}

// original 0x4F5BE0: with the frame-offset table the effects' (0x8E3580), a
// call through MagmaBreathRecord_Steps (four) by +2; then while +0 and +2 are
// not 0: the sprite on the screen, the record's matrix, its glow, the matrix
// popped; the battle's table back.
S37_EXPORT void __cdecl MagmaBreathRecord_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::MagmaBreathRecord_Launch, addr::MagmaBreathRecord_Fly,
                                                addr::MagmaBreathRecord_Land, addr::MagmaBreathRecord_Burn};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    Dispatch(kSteps, 4, Sc()[2], "MagmaBreathRecord_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) {
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        Call0(addr::MagmaBreathRecord_DrawGlow);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4F5C30: +9 down; at 0 the record starts: the acting actor's
// sprite's direction; (0x20000, 0) turned by it onto that sprite's point,
// 0x800000 above it; its target point (+0xC / +0x10 / +0x14) on a ring round
// the owner - radius ((+0xB >> 3) + 2) << 4 (dword 0x903850), angle
// (((+0xB >> 3) + 2 (+0xB & 7)) & 0xF) << 8 (dword 0x903854) - at the owner's
// height; its sprite fields, animation 0; +0x18 the angle to the target point
// (Math_Ratan2(dx, dz)); +9 3, +0xA 4, +2 on.
S37_EXPORT void __cdecl MagmaBreathRecord_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned char* const actor = Pointer(::bof3::Img(MagicS23_ActorSprite_addr));
    Sc()[8] = actor[8];
    rv::Current<records::BattleTask>()->step_x = 0x20000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    int v;
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(U(ts->step_x) + U(rv::As<records::SpriteHead>(actor)->x));
        ts->z = static_cast<std::int32_t>(U(ts->step_z) + U(rv::As<records::SpriteHead>(actor)->z));
        rv::Store(ts->height, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(actor)->height) + 0x800000u));
        const unsigned b = s[0xB];
        SetSD(0, ((b >> 3) + 2) << 4);
        const std::uint32_t angle = (((b >> 3) + (b & 7) * 2) & 0xF) << 8;
        SetSD(4, angle);
        v = Sin(static_cast<int>(angle));
    }
    {
        const std::uint32_t x = Mul(v, SD(0)) + U(rv::As<records::SpriteHead>(Owner())->x);
        rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(x));
    }
    v = Cos(SD(4));
    {
        const std::uint32_t z = Mul(v, SD(0)) + U(rv::As<records::SpriteHead>(Owner())->z);
        rv::Store(rv::Current<records::BattleTask>()->step_z, static_cast<std::int32_t>(z));
    }
    CopyLong(Sc() + 0x14, Owner() + 0x3C);
    {
        unsigned char* const s = Sc();
        s[0x25] = 0x1D;
        s[0x26] = 0xC0;
        rv::Store(rv::As<records::BattleTask>(s)->scale_x, 0x10000);
        rv::Store(rv::As<records::BattleTask>(s)->scale_y, 0x10000);
        s[0x48] = 0;
        s[0x27] = 2;
        s[0x28] = 1;
        s[0x24] = 0;
        s[0x5C] = 0;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
        s[0x2B] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const auto dz = static_cast<std::int32_t>(U(ts->step_z) - U(ts->z));
        const auto dx = static_cast<std::int32_t>(U(ts->step_x) - U(ts->x));
        const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        rv::Current<records::BattleTask>()->work_18 = angle;
    }
    Sc()[9] = 3;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x4F5E40: while +9 is not 0 it counts down and the sprite's script
// ticks (Sprite_ScriptTick, its answer unread); the last angle to +0x1C, the
// new one to +0x18 (Math_Ratan2 of the target point less the position); a
// step of 0x40 toward the target point (MagicFx_StepTowardPoint: its x / z
// >> 9 - 0x4000, its height's high word + 0xC0 halved); at the point
// (MagicFx_NearPoint, 0x8000) +2 on; else once the angle has turned by more
// than 0x600 and less than 0xA00 (|last & 0xFFF - new & 0xFFF|, left in +0x1C)
// - past it - +2 on.
S37_EXPORT void __cdecl MagmaBreathRecord_Fly(void) {
    {
        unsigned char* const s = Sc();
        const unsigned char c = s[9];
        if (c != 0) {
            s[9] = static_cast<unsigned char>(c - 1);
            MH_CALL(Sprite_ScriptTick)();
        }
    }
    std::int32_t px, pz;
    int x, z, y;
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        const std::int32_t h = ts->step_y;
        px = ts->step_x;
        pz = ts->step_z;
        CopyLong(s + 0x1C, s + 0x18);
        const auto dz = static_cast<std::int32_t>(U(pz) - U(ts->z));
        const auto dx = static_cast<std::int32_t>(U(px) - U(ts->x));
        x = (px >> 9) - 0x4000;
        z = (pz >> 9) - 0x4000;
        y = (static_cast<short>(U(h) >> 16) + 0xC0) >> 1;
        const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        rv::Current<records::BattleTask>()->work_18 = angle;
    }
    MH_CALL(MagicFx_StepTowardPoint)(U(x), U(z), U(y), 0, 0x40);
    if (MH_CALL(MagicFx_NearPoint)(U(px), U(pz), 0x8000) != 0) {
        Inc(Sc()[2]);
        return;
    }
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->work_1c = static_cast<std::int32_t>((U(ts->work_1c) & 0xFFF) - (U(ts->work_18) & 0xFFF));
    if (ts->work_1c < 0) ts->work_1c = static_cast<std::int32_t>(0u - U(ts->work_1c));
    const std::int32_t turned = ts->work_1c;
    if (turned > 0x600 && turned < 0xA00) Inc(s[2]);
}

// original 0x4F5F70: the record at its target point, its sprite fields,
// animation 1; +9 0xE, +0xA 0xC, +2 on.
S37_EXPORT void __cdecl MagmaBreathRecord_Land(void) {
    {
        unsigned char* const s = Sc();
        CopyLong(s + 0x34, s + 0xC);
        CopyLong(s + 0x38, s + 0x10);
        CopyLong(s + 0x3C, s + 0x14);
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x2B] = 1;
    }
    MH_CALL(Sprite_SetAnimation)(1);
    Sc()[9] = 0xE;
    Sc()[0xA] = 0xC;
    Inc(Sc()[2]);
}

// original 0x4F5FE0: the script ticked once (its answer unread); +0xA down
// unless 0; +9 down, at 0 the owner's count +0xB down and the record freed
// (MagicFx_FreeCurrentRecord, a tail jmp).
S37_EXPORT void __cdecl MagmaBreathRecord_Burn(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sc();
    if (s[0xA] != 0) Dec(s[0xA]);
    Dec(s[9]);
    if (s[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F6020: the record's matrix pushed: its x / z >> 9 - 0x4000,
// minus half the owner's height (+0x3E), no rotation - Camera_Matrix,
// translated by RotTrans of that point, set as the GTE's. Called by
// MAGIC053's LavaburstChild_Run (group S10) and GeoBreathChild_Run too.
S37_EXPORT void __cdecl MagicFx_PushRecordMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    short v[4];
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        v[0] = static_cast<short>((ts->x >> 9) - 0x4000);
        v[1] = static_cast<short>((ts->z >> 9) - 0x4000);
        const int height = S16(Owner() + 0x3E);
        v[2] = static_cast<short>(-(static_cast<int>(U(height) - U(height >> 31)) >> 1));
        v[3] = 0;
    }
    Matrix m;
    long flag;
    // The original pushes a third argument (a flag pointer) to Gte_RotTrans,
    // which takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S37_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4F60D0: a glow of 16 semi-transparent gouraud triangles round
// the origin, radius +0xA << 4 (the dword 0x903850), the centre colour 0x60,
// the rim 1; tpage 0x55, layer 5.
S37_EXPORT void __cdecl MagmaBreathRecord_DrawGlow(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSD(0, static_cast<std::uint32_t>(Sc()[0xA]) << 4);
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SD(0))));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SD(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SD(0))));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SD(0))));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = 0x60;
        p[5] = 0x60;
        p[6] = 0x60;
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// original 0x4F6230: the first of MagmaBreath_Pool's 32 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S37_EXPORT unsigned char __cdecl MagmaBreath_PoolAlloc(void) { return PoolAlloc(::bof3::Img(MagmaBreath_Pool_addr), kMagmaRecords); }

// original 0x4F6290: bytes +0..+4 of Sprite_Current cleared (Sprite_Current
// read again for each) - a pool record's free, bit 0 of +0 its "in use". The
// last step of records and children in 22 files, most by a tail jmp; al is
// left 0, which no caller reads.
S37_EXPORT void __cdecl MagicFx_FreeCurrentRecord(void) {
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

void MagmaBreathRecord_Inject() {
    BOF3_INJECT(MagmaBreath_Task);
    BOF3_INJECT(MagmaBreath_Start);
    BOF3_INJECT(MagmaBreath_Spawn);
    BOF3_INJECT(MagmaBreath_Strike);
    BOF3_INJECT(MagmaBreathRecord_Task);
    BOF3_INJECT(MagmaBreathRecord_Run);
    BOF3_INJECT(MagmaBreathRecord_Launch);
    BOF3_INJECT(MagmaBreathRecord_Fly);
    BOF3_INJECT(MagmaBreathRecord_Land);
    BOF3_INJECT(MagmaBreathRecord_Burn);
    BOF3_INJECT(MagicFx_PushRecordMatrix);
    BOF3_INJECT(MagmaBreathRecord_DrawGlow);
    BOF3_INJECT(MagmaBreath_PoolAlloc);
    BOF3_INJECT(MagicFx_FreeCurrentRecord);
}
