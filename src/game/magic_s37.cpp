// Three spell overlays compiled into the exe, round nine group S37
// (docs/magic_s37.md): the PSX's MAGIC219, MAGIC220/221 and MAGIC222.EMI,
// Magic_Rows rows 138, 136 / 146 and 133. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Magma Breath, Geo
// Breath / Gaea's Breath and Combustion; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC219 0x4F59D0..0x4F62BE: a pool of 32 records of its own
//     (0x6B4A60), 24 of them spawned round the side's centre, each flying at
//     its point, landing as a sprite that plays animation 1 and burns out
//     under a glow of 16 triangles; the target flagged 0x10. Its last two
//     functions are shared by 22 files: MagicFx_PushRecordMatrix (a record's
//     matrix at the owner's height) and MagicFx_FreeCurrentRecord (bytes
//     +0..+4 of Sprite_Current cleared);
//   - MAGIC220/221 0x4F62C0..0x4F71E6 (two rows, one code): a pool of 64
//     (0x6B5AE0) and one child (kind 1, 0x61) that rises over its owner,
//     swirls, and bursts sixteen records at a time while shaking and quaking
//     the field (Camera_ShiftX); the child draws a glow of 64 triangles and a
//     ring of 64 quads, each record a textured quad flying out and fading;
//   - MAGIC222 0x4F71F0..0x4F8638 (MAGIC222's BattleFx_Finish 0x4F7350 is
//     already ours): two children (kind 1, 0x60) - a copy of the owner's
//     sprite that falls from 0x7800000 above, shakes the camera, spawns eight
//     motes, a flash and fades, and a glow and ring that grow; the motes orbit
//     and draw a textured quad each, the flash a screen-space fan of four
//     gouraud quads a step, 32 steps.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s37.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace addr = bof3::addr;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kActorRecord = 0x904B3C;   // unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kFrameSet = 0x9039D8;      // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;

// The two pools of task-like records (0x84 bytes, like the task slots) the
// overlays walk from their kind-2 tasks, and MAGIC219's spawn delays (24
// bytes, read in place).
constexpr std::uint32_t kMagmaPool = 0x6B4A60;     // MagmaBreath_Pool, 32 records
constexpr unsigned kMagmaRecords = 32;
constexpr std::uint32_t kGeoPool = 0x6B5AE0;       // GeoBreath_Pool, 64 records
constexpr unsigned kGeoRecords = 64;
constexpr std::uint32_t kMagmaDelays = 0x65C22C;   // MagmaBreath_SpawnDelays

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(Long(at)) + v)); }
void CopyLong(unsigned char* to, const unsigned char* from) { SetLong(to, Long(from)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }
// `movsx` of a byte, then `shl 2`, as the word the originals store.
unsigned SignedShl2(unsigned char b) { return static_cast<unsigned>(static_cast<int>(static_cast<signed char>(b))) << 2; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* MagmaRecord(unsigned index) { return Mem(kMagmaPool + index * 0x84u); }
unsigned char* GeoRecord(unsigned index) { return Mem(kGeoPool + index * 0x84u); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
void Sound(unsigned id) { MH_CALL(Sound_PlayById)(static_cast<unsigned short>(id)); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using TaskFn = void (__cdecl*)(unsigned char*);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

// The phase handlers of MAGIC226/227 (group S38, not ours when this was
// written) a table holds: called by their addresses.
constexpr std::uint32_t kCountDownFlag10 = 0x4F9F70;   // +9 down, at 0 target flags 0x10 and +1 on
constexpr std::uint32_t kCombustionStep4 = 0x4FA390;   // CombustionSprite_Steps entry 4

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
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(task);
        SetLong(Mem(at::kOwner), owner);
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

// Gfx_ClutStrip's row 26 (0x1A00..0x1AFF) back from its source with the
// semi-transparency bit, then the first `row2` words of row 2 (0x200..) the
// same; the first word of each plain (26's first); the strip dirty.
void RestoreRow26ThenRow2(unsigned row2) {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x200 + row2; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    const unsigned short first26 = Gfx_ClutStripSource[0x1A00], first2 = Gfx_ClutStripSource[0x200];
    Gfx_ClutStrip[0x1A00] = first26;
    Gfx_ClutStrip[0x200] = first2;
    Gfx_ClutStripDirty = 1;
}

// The callees with the arguments the originals push: both projections get a
// flag pointer past the depth, which ours does not read.
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S37_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Prim_VertexScratch's first three SVECTORs projected to +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}
// All four projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}
// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
// Gfx_PacketNext's primitive of `size` linked at Sprite_Current's point.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                size);
}

// One MATRIX block as the originals lay it out on their stack: the rotation,
// then the translation RotTrans writes at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

// The draws' shared shape: a fan of 64 semi-transparent gouraud triangles
// round the origin (Prim_VertexScratch: the origin, the last rim point, the
// next at the angle; the radius the word 0x903850, all nine colour bytes the
// byte 0x90385A, both read again after every call), tpage 0x55, layer 5.
// The caller has set the two words.
void DrawGlowFan() {
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// The other: a ring of 64 semi-transparent gouraud quads between the radius
// word 0x903852 (vertices 0 and 1, colour 1) and the radius word 0x903850
// (vertices 2 and 3, the byte 0x90385A), tpage 0x55, layer 5.
void DrawRingBand() {
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Sin(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = Sin(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        {
            const int inner = Mul12(v, SS(2));
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(inner));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = Sin(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
}

// The textured quad of a record or mote: page (0x340, 0x100) abr 1, clut
// (0, 0x1E2), the texture's 0x20 x 0x20 at (0, 0x80); corners at the angles
// 0x200, 0x600, 0xE00, 0xA00, radius 0x100; the colour the byte 0x90385A
// (the word +0xA x `shade`, +0xA read after the packet is set up); tpage
// 0x35, linked at the sprite's point before and after.
void DrawTexturedQuad(unsigned shade) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    {
        const unsigned b = Sc()[0xA];
        SetSW(0, 0x100);
        SetSW(0xA, b * shade);
    }
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        SetVW(c * 8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(kAngles[c]);
        SetVW(c * 8 + 2, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(c * 8 + 4, 0);
    }
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1E2));
    p[0x15] = 0x80;
    p[0x25] = 0x80;
    p[0x14] = 0;
    p[0x24] = 0x20;
    p[0x34] = 0;
    p[0x35] = 0xA0;
    p[0x44] = 0x20;
    p[0x45] = 0xA0;
    p[4] = SB(0xA);
    p[5] = SB(0xA);
    p[6] = SB(0xA);
    Rtp4(p);
    MH_CALL(Gte_PrimDepths4_10)(p);
    LinkAtSprite(0x48);
}

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC219 (row 138, Magma Breath read one id down)

// original 0x4F59D0: the kind-2 task. A four-entry stack table by +1
// (MagmaBreath_Start, _Spawn, _Strike, BattleFx_Finish), unchecked; then the
// pool walked through MagmaBreathRecord_Task.
S37_EXPORT void __cdecl MagmaBreath_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::MagmaBreath_Start, addr::MagmaBreath_Spawn,
                                                 addr::MagmaBreath_Strike, addr::BattleFx_Finish};
    Dispatch(kPhases, 4, Sc()[1], "MagmaBreath_Task");
    WalkPool(kMagmaPool, kMagmaRecords, addr::MagmaBreathRecord_Task);
}

// original 0x4F5A60: the pool's +0..+2 cleared; the task at the side's
// centre; +0xB 0, +1 on; CLUT rows 2 and 26 back whole with their STP bits,
// the first word of each without; sound 0x100.
S37_EXPORT void __cdecl MagmaBreath_Start(void) {
    ClearPool(kMagmaPool, kMagmaRecords);
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
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(Sc())));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        rec[9] = static_cast<unsigned char>((r & 1) + Mem(kMagmaDelays)[i] + 1);
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
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 4, Sc()[2], "MagmaBreathRecord_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) {
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        Call0(addr::MagmaBreathRecord_DrawGlow);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
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
    const unsigned char* const actor = Pointer(kActorRecord);
    Sc()[8] = actor[8];
    SetLong(Sc() + 0xC, 0x20000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    int v;
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(U(Long(s + 0xC)) + U(Long(actor + 0x34))));
        SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(s + 0x10)) + U(Long(actor + 0x38))));
        SetLong(s + 0x3C, static_cast<std::int32_t>(U(Long(actor + 0x3C)) + 0x800000u));
        const unsigned b = s[0xB];
        SetSD(0, ((b >> 3) + 2) << 4);
        const std::uint32_t angle = (((b >> 3) + (b & 7) * 2) & 0xF) << 8;
        SetSD(4, angle);
        v = Sin(static_cast<int>(angle));
    }
    {
        const std::uint32_t x = Mul(v, SD(0)) + U(Long(Owner() + 0x34));
        SetLong(Sc() + 0xC, static_cast<std::int32_t>(x));
    }
    v = Cos(SD(4));
    {
        const std::uint32_t z = Mul(v, SD(0)) + U(Long(Owner() + 0x38));
        SetLong(Sc() + 0x10, static_cast<std::int32_t>(z));
    }
    CopyLong(Sc() + 0x14, Owner() + 0x3C);
    {
        unsigned char* const s = Sc();
        s[0x25] = 0x1D;
        s[0x26] = 0xC0;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
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
        SetWord(s + 0x2C, 0);
        s[0x2B] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    {
        const unsigned char* const s = Sc();
        const auto dz = static_cast<std::int32_t>(U(Long(s + 0x10)) - U(Long(s + 0x38)));
        const auto dx = static_cast<std::int32_t>(U(Long(s + 0xC)) - U(Long(s + 0x34)));
        const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        SetLong(Sc() + 0x18, angle);
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
        const std::int32_t h = Long(s + 0x14);
        px = Long(s + 0xC);
        pz = Long(s + 0x10);
        CopyLong(s + 0x1C, s + 0x18);
        const auto dz = static_cast<std::int32_t>(U(pz) - U(Long(s + 0x38)));
        const auto dx = static_cast<std::int32_t>(U(px) - U(Long(s + 0x34)));
        x = (px >> 9) - 0x4000;
        z = (pz >> 9) - 0x4000;
        y = (static_cast<short>(U(h) >> 16) + 0xC0) >> 1;
        const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        SetLong(Sc() + 0x18, angle);
    }
    MH_CALL(MagicFx_StepTowardPoint)(U(x), U(z), U(y), 0, 0x40);
    if (MH_CALL(MagicFx_NearPoint)(U(px), U(pz), 0x8000) != 0) {
        Inc(Sc()[2]);
        return;
    }
    unsigned char* const s = Sc();
    SetLong(s + 0x1C, static_cast<std::int32_t>((U(Long(s + 0x1C)) & 0xFFF) - (U(Long(s + 0x18)) & 0xFFF)));
    if (Long(s + 0x1C) < 0) SetLong(s + 0x1C, static_cast<std::int32_t>(0u - U(Long(s + 0x1C))));
    const std::int32_t turned = Long(s + 0x1C);
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
        v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
        v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
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
S37_EXPORT unsigned char __cdecl MagmaBreath_PoolAlloc(void) { return PoolAlloc(kMagmaPool, kMagmaRecords); }

// original 0x4F6290: bytes +0..+4 of Sprite_Current cleared (Sprite_Current
// read again for each) - a pool record's free, bit 0 of +0 its "in use". The
// last step of records and children in 22 files, most by a tail jmp; al is
// left 0, which no caller reads.
S37_EXPORT void __cdecl MagicFx_FreeCurrentRecord(void) {
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// ===========================================================================
// MAGIC220 / MAGIC221 (rows 136 and 146, Geo Breath and Gaea's Breath read
// one id down: one code)

// original 0x4F62C0: the kind-2 task. A three-entry stack table by +1
// (GeoBreath_Start, MAGIC226/227's 0x4F9F70, BattleFx_Finish), unchecked;
// then the pool walked through GeoBreathRecord_Task.
S37_EXPORT void __cdecl GeoBreath_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::GeoBreath_Start, kCountDownFlag10, addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "GeoBreath_Task");
    WalkPool(kGeoPool, kGeoRecords, addr::GeoBreathRecord_Task);
}

// original 0x4F6340: the pool's +0..+2 cleared; the task at the side's
// centre, then at the field's kind-2 point (Field_Kind2X / Z); +0xB 0, +9
// 0x38, +1 on; sound 0x100; one child (kind 1, 0x61), +0x80 this task, +1 0,
// counted in +0xB; CLUT row 26 back whole and the first 16 words of row 2
// with their STP bits, the first word of each without.
S37_EXPORT void __cdecl GeoBreath_Start(void) {
    ClearPool(kGeoPool, kGeoRecords);
    MH_CALL(MagicFx_CenterOnSide)();
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, Field_Kind2X);
        SetLong(s + 0x38, Field_Kind2Z);
        s[0xB] = 0;
        s[9] = 0x38;
        Inc(s[1]);
    }
    Sound(0x100);
    {
        const unsigned slot = NewTask(0x61);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 0;
        Inc(self[0xB]);
    }
    RestoreRow26ThenRow2(0x10);
}

// original 0x4F6440: the child's kind-1 task, a jmp through
// GeoBreathChild_TaskTable (one entry) by +1, unchecked.
S37_EXPORT void __cdecl GeoBreathChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::GeoBreathChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "GeoBreathChild_Task");
}

// original 0x4F6460: with the frame-offset table the effects', a call through
// GeoBreathChild_Steps (eight: _Start, _Swirl, _Burst, _Shake, _Quake,
// _Settle, group S18's BuffRing_Wait, _End) by +2; then while bit 0 of +0 is
// set and +2 is not 0: the sprite on the screen, the record matrix, the glow,
// the ring, the matrix popped; the battle's table back.
S37_EXPORT void __cdecl GeoBreathChild_Run(void) {
    static constexpr std::uint32_t kSteps[8] = {
        addr::GeoBreathChild_Start, addr::GeoBreathChild_Swirl,  addr::GeoBreathChild_Burst, addr::GeoBreathChild_Shake,
        addr::GeoBreathChild_Quake, addr::GeoBreathChild_Settle, addr::BuffRing_Wait,         addr::GeoBreathChild_End};
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 8, Sc()[2], "GeoBreathChild_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0) {
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        Call0(addr::GeoBreathChild_DrawGlow);
        Call0(addr::GeoBreathChild_DrawRing);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4F64C0: the child at its owner's point, 0x1600000 above it; its
// sprite fields, animation 0; +0x5D 0x10, +0xB 0, +9 0xA, +0xA 0, +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Start(void) {
    {
        unsigned char* const s = Sc();
        CopyLong(s + 0x34, Owner() + 0x34);
        CopyLong(s + 0x38, Owner() + 0x38);
        SetLong(s + 0x3C, static_cast<std::int32_t>(U(Long(Owner() + 0x3C)) + 0x1600000u));
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x28] = 1;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
        s[0x48] = 0;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        SetWord(s + 0x2C, 0);
        s[0x2B] = 0;
        s[0x24] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    unsigned char* const s = Sc();
    s[0x5D] = 0x10;
    s[0xB] = 0;
    s[9] = 0xA;
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4F65D0: the script ticked once (its answer unread); the glow's
// radius +0xB up by 0xC; +9 down, at 0 +9 0x1E, +0xA 3 (bursts) and +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Swirl(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 0xC);
    Dec(s[9]);
    if (s[9] != 0) return;
    s[9] = 0x1E;
    s[0xA] = 3;
    Inc(s[2]);
}

namespace {

// Sixteen GeoBreath_Pool records (a full pool skips one): +0x80 the owner
// (the kind-2 task), +1 0, +0xB the number; the owner's count +0xB up.
void SpawnGeoRecords() {
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned index = MH_AT(ByteFn, addr::GeoBreath_PoolAlloc)() & 0xFFu;
        if (index == 0xFF) continue;
        unsigned char* const rec = GeoRecord(index);
        unsigned char* const owner = Owner();
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(owner)));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        Inc(owner[0xB]);
    }
}

// The child shaken about its owner's point by +0xC: (+, -) on odd frames
// (Frame_Counter's bit 0), (-, +) on even ones.
void ShakeAboutOwner() {
    unsigned char* const s = Sc();
    const std::uint32_t d = U(Long(s + 0xC));
    if ((Frame_Counter & 1) != 0) {
        SetLong(s + 0x34, static_cast<std::int32_t>(U(Long(Owner() + 0x34)) + d));
        SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(Owner() + 0x38)) - d));
    } else {
        SetLong(s + 0x34, static_cast<std::int32_t>(U(Long(Owner() + 0x34)) - d));
        SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(Owner() + 0x38)) + d));
    }
}

// Every eighth frame (Frame_Counter & 7 0): sound 0x102 and sixteen records.
void BurstOnEighthFrame() {
    if ((Frame_Counter & 7) != 0) return;
    Sound(0x102);
    SpawnGeoRecords();
}

}  // namespace

// original 0x4F6620: +9 down; at 0 +0xA (bursts left) down: at 0
// MapView_Redraw 0x5D, +9 0x3C and +2 4 (the quake); else sixteen records,
// sound 0x101 at the second-last burst, sound 0x102, the shake +0xC 0x800,
// +9 4, +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Burst(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        unsigned char* const s = Sc();
        Dec(s[0xA]);
        if (s[0xA] == 0) {
            MapView_Redraw = 0x5D;
            s[9] = 0x3C;
            Sc()[2] = 4;
            return;
        }
    }
    SpawnGeoRecords();
    if (Sc()[0xA] == 2) Sound(0x101);
    Sound(0x102);
    SetLong(Sc() + 0xC, 0x800);
    Sc()[9] = 4;
    Inc(Sc()[2]);
}

// original 0x4F6700: shaken about the owner; +9 down, at 0 +9 8 and +2 back
// one (the next burst).
S37_EXPORT void __cdecl GeoBreathChild_Shake(void) {
    ShakeAboutOwner();
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0) return;
    s[9] = 8;
    Dec(Sc()[2]);
}

// original 0x4F6780: shaken; every eighth frame a burst; Camera_ShiftX 0x10
// on odd +9, 0 on even; +9 down, at 0 +9 0x20 and +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Quake(void) {
    ShakeAboutOwner();
    BurstOnEighthFrame();
    unsigned char* const s = Sc();
    Camera_ShiftX = static_cast<short>((s[9] & 1u) << 4);
    Dec(s[9]);
    if (s[9] != 0) return;
    s[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4F6880: shaken; every eighth frame a burst; Camera_ShiftX +9 >>
// 1 on odd +9, 0 on even; +9 down, at 0 Camera_ShiftX 0, +9 8 and +2 on.
S37_EXPORT void __cdecl GeoBreathChild_Settle(void) {
    ShakeAboutOwner();
    BurstOnEighthFrame();
    unsigned char* const s = Sc();
    const unsigned char c = s[9];
    Camera_ShiftX = static_cast<short>((c & 1) != 0 ? c >> 1 : 0);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    Camera_ShiftX = 0;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F6990: the glow's shade +0x5D down by 2 unless 0; the script
// ticked once, and at its end the owner's count +0xB down and the task freed
// (a tail jmp).
S37_EXPORT void __cdecl GeoBreathChild_End(void) {
    {
        unsigned char* const s = Sc();
        if (s[0x5D] != 0) s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
    }
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFFu) == 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F69C0: a glow of 64 semi-transparent gouraud triangles round
// the origin, radius +0xB, every colour byte the signed +0x5D x 4; tpage 0x55,
// layer 5 (the fan of MAGIC053's LavaburstChild_DrawGlow with its own words).
S37_EXPORT void __cdecl GeoBreathChild_DrawGlow(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(0xA, SignedShl2(s[0x5D]));
    }
    DrawGlowFan();
}

// original 0x4F6B60: a ring of 64 semi-transparent gouraud quads from radius
// +0xB x 2 (dark) in to +0xB (the signed +0x5D x 4); tpage 0x55, layer 5.
S37_EXPORT void __cdecl GeoBreathChild_DrawRing(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(2, static_cast<unsigned>(s[0xB]) << 1);
        SetSW(0xA, SignedShl2(s[0x5D]));
    }
    DrawRingBand();
}

// original 0x4F6D90: a record's task, a jmp through GeoBreathRecord_TaskTable
// (one entry) by +1, unchecked.
S37_EXPORT void __cdecl GeoBreathRecord_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::GeoBreathRecord_Run};
    Dispatch(kKinds, 1, Sc()[1], "GeoBreathRecord_Task");
}

// original 0x4F6DB0: a call through GeoBreathRecord_Steps (three) by +2; then
// while +0 and +2 are not 0 the actor matrix, the quad, the matrix popped (a
// tail jmp).
S37_EXPORT void __cdecl GeoBreathRecord_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::GeoBreathRecord_Start, addr::GeoBreathRecord_Fly,
                                                addr::GeoBreathRecord_Fade};
    Dispatch(kSteps, 3, Sc()[2], "GeoBreathRecord_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::GeoBreathRecord_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4F6DF0: the record at the angle +0xB << 8 (the word 0x903854)
// round its owner, radius 24 (sin / cos x 24, unshifted), at the owner's
// height; +9 8, +0xA 0x10, +2 on.
S37_EXPORT void __cdecl GeoBreathRecord_Start(void) {
    const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(Sc()[0xB]) << 8);
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(U(Long(Owner() + 0x34)) + Mul(v, 24)));
    v = Cos(SS(4));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(U(Long(Owner() + 0x38)) + Mul(v, 24)));
    CopyLong(Sc() + 0x3C, Owner() + 0x3C);
    unsigned char* const s = Sc();
    s[9] = 8;
    s[0xA] = 0x10;
    Inc(s[2]);
}

namespace {

// The record moved out along the angle +0xB << 8 (the word 0x903854) by
// sin / cos x `times`: each cell's address taken before its call, as the
// original holds it in esi.
void MoveAlongAngle(int times) {
    {
        unsigned char* const s = Sc();
        const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(s[0xB]) << 8);
        SetSW(4, angle);
        unsigned char* const cell = s + 0x34;
        const int v = Sin(static_cast<short>(angle));
        AddLong(cell, Mul(v, times));
    }
    {
        const short angle = SS(4);
        unsigned char* const cell = Sc() + 0x38;
        const int v = Cos(angle);
        AddLong(cell, Mul(v, times));
    }
}

}  // namespace

// original 0x4F6E80: out by sin / cos x 3; +9 down, at 0 +2 on.
S37_EXPORT void __cdecl GeoBreathRecord_Fly(void) {
    MoveAlongAngle(3);
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4F6EF0: out by sin / cos x 2; the shade +0xA down by 2, at 0 the
// owner's count +0xB down and the record freed (MagicFx_FreeCurrentRecord, a
// tail jmp).
S37_EXPORT void __cdecl GeoBreathRecord_Fade(void) {
    MoveAlongAngle(2);
    unsigned char* const s = Sc();
    s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F6F70: the textured quad, its colour +0xA x 6.
S37_EXPORT void __cdecl GeoBreathRecord_Draw(void) { DrawTexturedQuad(6); }

// original 0x4F7190: the first of GeoBreath_Pool's 64 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S37_EXPORT unsigned char __cdecl GeoBreath_PoolAlloc(void) { return PoolAlloc(kGeoPool, kGeoRecords); }

// ===========================================================================
// MAGIC222 (row 133, Combustion read one id down)

// original 0x4F71F0: the kind-2 task. A three-entry stack table by +1
// (Combustion_Start, _Wait, BattleFx_Finish), unchecked.
S37_EXPORT void __cdecl Combustion_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Combustion_Start, addr::Combustion_Wait, addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Combustion_Task");
}

// original 0x4F7220: the task at the side's centre and on the screen; +0xB 0,
// +9 0x10, +1 on; two children (kind 1, 0x60), +0x80 this task, +9 0x10 - the
// glow (+1 1) and the sprite (+1 0) - counted in +0xB; CLUT row 26 back whole
// and the first 16 words of row 2 with their STP bits, the first word of each
// without.
S37_EXPORT void __cdecl Combustion_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
    for (unsigned kind : {1u, 0u}) {
        const unsigned slot = NewTask(0x60);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = static_cast<unsigned char>(kind);
        child[9] = 0x10;
        Inc(self[0xB]);
    }
    RestoreRow26ThenRow2(0x10);
}

// original 0x4F7320: +9 down; at 0 sound 0x100 and +1 on.
S37_EXPORT void __cdecl Combustion_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sound(0x100);
    Inc(Sc()[1]);
}

// original 0x4F7380: the children's kind-1 task, a jmp through
// CombustionChild_Kinds (four: the sprite, the glow, a mote, the flash) by
// +1, unchecked.
S37_EXPORT void __cdecl CombustionChild_Task(void) {
    static constexpr std::uint32_t kKinds[4] = {addr::CombustionSprite_Run, addr::CombustionGlow_Run,
                                                addr::CombustionMote_Run, addr::CombustionFlash_Run};
    Dispatch(kKinds, 4, Sc()[1], "CombustionChild_Task");
}

// original 0x4F73A0: with the frame-offset table the effects', a call through
// CombustionSprite_Steps (six: _Start, _Fall, _Shake, _Flash, MAGIC226/227's
// 0x4FA390, _Fade) by +2; then while bit 0 of +0 is set and +2 is not 0 the
// screen point and the sprite on the screen; the battle's table back.
S37_EXPORT void __cdecl CombustionSprite_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {addr::CombustionSprite_Start, addr::CombustionSprite_Fall,
                                                addr::CombustionSprite_Shake, addr::CombustionSprite_Flash,
                                                kCombustionStep4,             addr::CombustionSprite_Fade};
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 6, Sc()[2], "CombustionSprite_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(Sprite_UpdateScreen)();
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4F73F0: +9 down; at 0 the sprite starts 0x7800000 over its
// owner's point, falling by 0x600000; its sprite fields, animation 0; +9 0x10,
// +2 on.
S37_EXPORT void __cdecl CombustionSprite_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x14, 0x7800000);
        SetLong(s + 0x20, 0x600000);
        CopyLong(s + 0x34, Owner() + 0x34);
        CopyLong(s + 0x38, Owner() + 0x38);
        SetLong(s + 0x3C, static_cast<std::int32_t>(U(Long(Owner() + 0x3C)) + U(Long(s + 0x14))));
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x28] = 1;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
        s[0x48] = 2;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        SetWord(s + 0x2C, 0);
        s[0x2B] = 1;
        s[0x24] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4F7520: the height +0x14 down by the fall +0x20, set over the
// owner's; +9 down; at 0 sound 0x101, eight motes (kind 1, 0x60; +0x80 the
// owner, +1 2, +0xB the number; the owner's count up), the camera's first
// angle up by 0x14, +9 8, +2 on.
S37_EXPORT void __cdecl CombustionSprite_Fall(void) {
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x14, static_cast<std::int32_t>(U(Long(s + 0x14)) - U(Long(s + 0x20))));
        SetLong(s + 0x3C, static_cast<std::int32_t>(U(Long(Owner() + 0x3C)) + U(Long(s + 0x14))));
        Dec(s[9]);
        if (s[9] != 0) return;
    }
    Sound(0x101);
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x60);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(owner)));
        child[1] = 2;
        child[0xB] = static_cast<unsigned char>(i);
        Inc(owner[0xB]);
    }
    unsigned char* const s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + 0x14);
    s[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F75E0: the camera's first angle up by 0x14 on odd +9, down on
// even; +9 down, at 0 the angle 0xFD56, +9 0x16, +2 on.
S37_EXPORT void __cdecl CombustionSprite_Shake(void) {
    unsigned char* const s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + ((s[9] & 1) != 0 ? 0x14 : -0x14));
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    Camera_Angles[0] = static_cast<short>(0xFD56);
    Sc()[9] = 0x16;
    Inc(Sc()[2]);
}

// original 0x4F7630: +9 down; at 0 sound 0x102, the flash (kind 1, 0x60;
// +0x80 the owner, +1 3; the owner's count up); the sprite's +0 bit 0x20,
// +0x5C 1, +0x5D..+0x5F 0x40; +9 0xC, +2 on.
S37_EXPORT void __cdecl CombustionSprite_Flash(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sound(0x102);
    {
        const unsigned slot = NewTask(0x60);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(owner)));
        child[1] = 3;
        Inc(owner[0xB]);
    }
    unsigned char* const s = Sc();
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s[0x5C] = 1;
    s[0x5D] = 0x40;
    s[0x5E] = 0x40;
    s[0x5F] = 0x40;
    s[9] = 0xC;
    Inc(s[2]);
}

// original 0x4F76E0: +0x5D..+0x5F down by 0x10 each (add 0xF0), the scale
// +0x40 up by 0x1000 and again by 0x800 (+0x44 untouched); at +0x5D 0x80 the
// owner's count +0xB down and the task freed (a tail jmp).
S37_EXPORT void __cdecl CombustionSprite_Fade(void) {
    unsigned char* const s = Sc();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xF0);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xF0);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xF0);
    AddLong(s + 0x40, 0x1000);
    AddLong(s + 0x40, 0x800);
    if (s[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F7750: a call through CombustionGlow_Steps (four: _Start, _Grow,
// group S17's MagicFx_WaitA, group S12's MagicFx_CountDownRelease) by +2;
// then while +0 and +2 are not 0 the actor matrix, the glow, the ring, the
// matrix popped (a tail jmp).
S37_EXPORT void __cdecl CombustionGlow_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::CombustionGlow_Start, addr::CombustionGlow_Grow, addr::MagicFx_WaitA,
                                                addr::MagicFx_CountDownRelease};
    Dispatch(kSteps, 4, Sc()[2], "CombustionGlow_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::CombustionGlow_DrawGlow);
    Call0(addr::CombustionGlow_DrawRing);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4F7790: +9 down; at 0 the glow at its owner's point, the radius
// +0x14 0, +9 0, +0xA 0x26, +2 on.
S37_EXPORT void __cdecl CombustionGlow_Start(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    CopyLong(s + 0x34, Owner() + 0x34);
    CopyLong(s + 0x38, Owner() + 0x38);
    CopyLong(s + 0x3C, Owner() + 0x3C);
    SetLong(s + 0x14, 0);
    s[9] = 0;
    s[0xA] = 0x26;
    Inc(s[2]);
}

// original 0x4F7800: +9 (the shade) up, the radius +0x14 up by 6; at +9 0x10
// +2 on.
S37_EXPORT void __cdecl CombustionGlow_Grow(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    AddLong(s + 0x14, 6);
    if (s[9] == 0x10) Inc(s[2]);
}

// original 0x4F7830: the glow fan, radius the word +0x14, colour +9 x 4.
S37_EXPORT void __cdecl CombustionGlow_DrawGlow(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, Word(s + 0x14));
        SetSW(0xA, static_cast<unsigned>(s[9]) << 2);
    }
    DrawGlowFan();
}

// original 0x4F79D0: the ring, from radius (the word +0x14) x 2 (dark) in to
// the word +0x14 (colour +9 x 4).
S37_EXPORT void __cdecl CombustionGlow_DrawRing(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, Word(s + 0x14));
        SetSW(2, static_cast<unsigned>(Word(s + 0x14)) << 1);
        SetSW(0xA, static_cast<unsigned>(s[9]) << 2);
    }
    DrawRingBand();
}

// original 0x4F7C00: a call through CombustionMote_Steps (three) by +2; then
// while +0 and +2 are not 0 the actor matrix, the quad, the matrix popped (a
// tail jmp). MAGIC053's LavaburstRecord_Steps holds this unit's
// CombustionMote_Start too.
S37_EXPORT void __cdecl CombustionMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::CombustionMote_Start, addr::CombustionMote_Grow,
                                                addr::CombustionMote_Shrink};
    Dispatch(kSteps, 3, Sc()[2], "CombustionMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::CombustionMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

namespace {

// The mote on its orbit: at the angle +0xB << 9 (the word 0x903854) round its
// owner, radius +0xC (no shift), Sprite_Current read again after each call.
void MoteOrbit() {
    const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(Sc()[0xB]) << 9);
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(Mul(v, Long(s + 0xC)) + U(Long(Owner() + 0x34))));
    }
    v = Cos(SS(4));
    unsigned char* const s = Sc();
    SetLong(s + 0x38, static_cast<std::int32_t>(Mul(v, Long(s + 0xC)) + U(Long(Owner() + 0x38))));
}

}  // namespace

// original 0x4F7C40: the radius +0xC (Rand & 7) + 8, the mote on its orbit at
// its owner's height; +9 8, +0xA 0x10, +2 on.
S37_EXPORT void __cdecl CombustionMote_Start(void) {
    const std::uint32_t r = RandCall();
    SetLong(Sc() + 0xC, static_cast<std::int32_t>((r & 7) + 8));
    MoteOrbit();
    CopyLong(Sc() + 0x3C, Owner() + 0x3C);
    unsigned char* const s = Sc();
    s[9] = 8;
    s[0xA] = 0x10;
    Inc(s[2]);
}

// original 0x4F7CE0: the radius up by 1, the orbit; +9 down, at 0 +2 on.
S37_EXPORT void __cdecl CombustionMote_Grow(void) {
    AddLong(Sc() + 0xC, 1);
    MoteOrbit();
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4F7D70: the radius up by 1, the orbit; +0xA down, at 0 the
// owner's count +0xB down and the task freed (a tail jmp).
S37_EXPORT void __cdecl CombustionMote_Shrink(void) {
    AddLong(Sc() + 0xC, 1);
    MoteOrbit();
    unsigned char* const s = Sc();
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F7E10: the textured quad, its colour +0xA x 8.
S37_EXPORT void __cdecl CombustionMote_Draw(void) { DrawTexturedQuad(8); }

// original 0x4F8030: a call through CombustionFlash_Steps (five: _Start,
// group S30's MagicFx_CountUp9By2, _Hold, _Spin, _End) by +2; then while +0
// and +2 are not 0 the flash drawn twice (a call, then a tail jmp).
S37_EXPORT void __cdecl CombustionFlash_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::CombustionFlash_Start, addr::MagicFx_CountUp9By2,
                                                addr::CombustionFlash_Hold, addr::CombustionFlash_Spin,
                                                addr::CombustionFlash_End};
    Dispatch(kSteps, 5, Sc()[2], "CombustionFlash_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(addr::CombustionFlash_Draw);
    Call0(addr::CombustionFlash_Draw);
}

// original 0x4F8060: the flash at its owner's screen point (words +0x2E /
// +0x30); the target flagged 0x10; +0xB 8, +9 0, +0xA 4, +2 on.
S37_EXPORT void __cdecl CombustionFlash_Start(void) {
    {
        unsigned char* const s = Sc();
        SetWord(s + 0x2E, Word(Owner() + 0x2E));
        SetWord(s + 0x30, Word(Owner() + 0x30));
    }
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    unsigned char* const s = Sc();
    s[0xB] = 8;
    s[9] = 0;
    s[0xA] = 4;
    Inc(s[2]);
}

// original 0x4F80C0: +0xA down, at 0 +0xA 0x10 and +2 on.
S37_EXPORT void __cdecl CombustionFlash_Hold(void) {
    unsigned char* const s = Sc();
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    s[0xA] = 0x10;
    Inc(s[2]);
}

// original 0x4F80F0: the radius +0xB up by 4; +0xA down, at 0 +2 on.
S37_EXPORT void __cdecl CombustionFlash_Spin(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 4);
    Dec(s[0xA]);
    if (s[0xA] == 0) Inc(s[2]);
}

// original 0x4F8120: the radius +0xB up by 4; +9 down, at 0 the owner's count
// +0xB down and the task freed (a tail jmp).
S37_EXPORT void __cdecl CombustionFlash_End(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 4);
    Dec(s[9]);
    if (s[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F8160: a draw-mode packet (tpage 0x35) on layer 2; the words
// 0x903850 the radius +0xB, 0x903852 twice it, 0x90385A +9 x 15, 0x90385C and
// 0x90385E +9 x 8; then 32 steps across the screen from the sprite's screen
// point less 0x100 (the word +0x2E), 0x10 wide, the angle 0x40 a step: four
// semi-transparent gouraud quads a step in screen space (floats), each on
// layer 2 - above the line y (the word +0x30) a band to r sin (grey at the
// line, the three colours at the edge) and a band from r sin to 2r sin (the
// three colours inside, 1 outside), and the same two below. The radius words
// are read again after every Math_Sin, as the original reads them.
S37_EXPORT void __cdecl CombustionFlash_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    std::uint32_t step_x;
    int y;
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(2, static_cast<unsigned>(s[0xB]) << 1);
        SetSW(0xA, s[9] * 15u);
        SetSW(0xC, static_cast<unsigned>(s[9]) << 3);
        SetSW(0xE, static_cast<unsigned>(s[9]) << 3);
        step_x = (Word(s + 0x2E) - 0x100u) & 0xFFFF;
        y = S16(s + 0x30);
    }
    unsigned char fy[4];
    PutFloat(fy, y);
    // The three colour bytes, read at each store.
    auto top = [](unsigned char* p, unsigned at) {
        p[at] = SB(0xA);
        p[at + 1] = SB(0xC);
        p[at + 2] = SB(0xE);
    };
    auto grey = [](unsigned char* p, unsigned at) {
        p[at] = SB(0xA);
        p[at + 1] = SB(0xA);
        p[at + 2] = SB(0xA);
    };
    auto dark = [](unsigned char* p, unsigned at) {
        p[at] = 1;
        p[at + 1] = 1;
        p[at + 2] = 1;
    };
    int a = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        const int x0 = static_cast<short>(step_x);
        const int x1 = x0 + 0x10;
        const int a1 = a + 0x40;
        unsigned char f0[4], f1[4];
        PutFloat(f0, x0);
        PutFloat(f1, x1);
        // above the line, the inner band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int ya = y + Mul12(v, SS(0));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, ya);
            std::memcpy(p + 0x2C, fy, 4);
            std::memcpy(p + 0x18, f1, 4);
            v = Sin(a1);
            const int yb = y + Mul12(v, SS(0));
            std::memcpy(p + 0x38, f1, 4);
            std::memcpy(p + 0x3C, fy, 4);
            PutFloat(p + 0x1C, yb);
            top(p, 4);
            top(p, 0x14);
            grey(p, 0x24);
            grey(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        // above, the outer band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int y0 = y + Mul12(v, SS(2));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, y0);
            v = Sin(a);
            const int y2 = y + Mul12(v, SS(0));
            std::memcpy(p + 0x18, f1, 4);
            PutFloat(p + 0x2C, y2);
            v = Sin(a1);
            const int y1 = y + Mul12(v, SS(2));
            std::memcpy(p + 0x38, f1, 4);
            PutFloat(p + 0x1C, y1);
            v = Sin(a1);
            const int y3 = y + Mul12(v, SS(0));
            dark(p, 4);
            dark(p, 0x14);
            PutFloat(p + 0x3C, y3);
            top(p, 0x24);
            top(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        // below the line, the inner band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int ya = y - Mul12(v, SS(0));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, ya);
            std::memcpy(p + 0x2C, fy, 4);
            std::memcpy(p + 0x18, f1, 4);
            v = Sin(a1);
            const int yb = y - Mul12(v, SS(0));
            std::memcpy(p + 0x38, f1, 4);
            std::memcpy(p + 0x3C, fy, 4);
            PutFloat(p + 0x1C, yb);
            top(p, 4);
            top(p, 0x14);
            grey(p, 0x24);
            grey(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        // below, the outer band
        {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            std::memcpy(p + 8, f0, 4);
            int v = Sin(a);
            const int y0 = y - Mul12(v, SS(2));
            std::memcpy(p + 0x28, f0, 4);
            PutFloat(p + 0xC, y0);
            v = Sin(a);
            const int y2 = y - Mul12(v, SS(0));
            std::memcpy(p + 0x18, f1, 4);
            PutFloat(p + 0x2C, y2);
            v = Sin(a1);
            const int y1 = y - Mul12(v, SS(2));
            std::memcpy(p + 0x38, f1, 4);
            PutFloat(p + 0x1C, y1);
            v = Sin(a1);
            const int y3 = y - Mul12(v, SS(0));
            dark(p, 4);
            dark(p, 0x14);
            PutFloat(p + 0x3C, y3);
            top(p, 0x24);
            top(p, 0x34);
            MH_CALL(Gfx_CommitPrim)(2, 0x44);
        }
        step_x += 0x10;
        a = a1;
    }
}

void MagicS37_Inject() {
    if (bof3::WantsShadow("magic_s37")) magic_s37::SelfTest();
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
    BOF3_INJECT(GeoBreath_Task);
    BOF3_INJECT(GeoBreath_Start);
    BOF3_INJECT(GeoBreathChild_Task);
    BOF3_INJECT(GeoBreathChild_Run);
    BOF3_INJECT(GeoBreathChild_Start);
    BOF3_INJECT(GeoBreathChild_Swirl);
    BOF3_INJECT(GeoBreathChild_Burst);
    BOF3_INJECT(GeoBreathChild_Shake);
    BOF3_INJECT(GeoBreathChild_Quake);
    BOF3_INJECT(GeoBreathChild_Settle);
    BOF3_INJECT(GeoBreathChild_End);
    BOF3_INJECT(GeoBreathChild_DrawGlow);
    BOF3_INJECT(GeoBreathChild_DrawRing);
    BOF3_INJECT(GeoBreathRecord_Task);
    BOF3_INJECT(GeoBreathRecord_Run);
    BOF3_INJECT(GeoBreathRecord_Start);
    BOF3_INJECT(GeoBreathRecord_Fly);
    BOF3_INJECT(GeoBreathRecord_Fade);
    BOF3_INJECT(GeoBreathRecord_Draw);
    BOF3_INJECT(GeoBreath_PoolAlloc);
    BOF3_INJECT(Combustion_Task);
    BOF3_INJECT(Combustion_Start);
    BOF3_INJECT(Combustion_Wait);
    BOF3_INJECT(CombustionChild_Task);
    BOF3_INJECT(CombustionSprite_Run);
    BOF3_INJECT(CombustionSprite_Start);
    BOF3_INJECT(CombustionSprite_Fall);
    BOF3_INJECT(CombustionSprite_Shake);
    BOF3_INJECT(CombustionSprite_Flash);
    BOF3_INJECT(CombustionSprite_Fade);
    BOF3_INJECT(CombustionGlow_Run);
    BOF3_INJECT(CombustionGlow_Start);
    BOF3_INJECT(CombustionGlow_Grow);
    BOF3_INJECT(CombustionGlow_DrawGlow);
    BOF3_INJECT(CombustionGlow_DrawRing);
    BOF3_INJECT(CombustionMote_Run);
    BOF3_INJECT(CombustionMote_Start);
    BOF3_INJECT(CombustionMote_Grow);
    BOF3_INJECT(CombustionMote_Shrink);
    BOF3_INJECT(CombustionMote_Draw);
    BOF3_INJECT(CombustionFlash_Run);
    BOF3_INJECT(CombustionFlash_Start);
    BOF3_INJECT(CombustionFlash_Hold);
    BOF3_INJECT(CombustionFlash_Spin);
    BOF3_INJECT(CombustionFlash_End);
    BOF3_INJECT(CombustionFlash_Draw);
}
