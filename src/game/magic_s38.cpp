// Three spell overlays compiled into the exe, round nine group S38
// (docs/magic_s38.md): the PSX's MAGIC223, MAGIC225 and MAGIC226/227.EMI,
// Magic_Rows rows 137, 140, 135 and 147. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Tempest / Hurricane
// (MAGIC223 serves both ids), an id with no label (MAGIC225) and MeteorStrike
// (MAGIC226; MAGIC227 is row 147, ability 227, which runs the same code); the
// names below use those labels as hypotheses and say what the code does.
//
//   - MAGIC223 0x4F8640..0x4F8F36: a full-screen flash (kind 1, 0x63) and a
//     pool of 64 gusts of its own (0x6B7BE0), 48 of them started round the
//     owner with delays; each drifts off at an angle by the owner's direction
//     and draws one textured quad, its CLUT row by the ability (0xDF or not);
//   - MAGIC225 0x4F8F40..0x4F9DD6: a veil (kind 1, 0x65) that moves the
//     target side's actors to a draw layer, shades the screen and draws four
//     coloured bursts at the screen's corners; 16 shards from a pool of 32
//     (0x6B9CE0) fly from the actor toward the task; then three stats of
//     MagicFx_BuffStats applied to every live actor of the side, each with a
//     popup (kind 1, 2);
//   - MAGIC226/227 0x4F9DE0..0x4FAFE6: a rock (kind 1, 0x5F) that falls from
//     high over the target side leaving a trail, lands with 32 chips, shakes
//     the camera and fades; trail and chips share a pool of 48 (0x6BAD60).
//     Its 0x4F9F70 (+9 down, then target flags 0x10) is a phase ten overlays'
//     tables hold, and 0x4FA440 (a ground ring) MAGIC053's too.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent). The .data value
// tables (the gust angles, the burst colours, the chips' shades and lifts)
// are read in place by any index, as the originals read them.
#include "game/magic_s38.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
namespace addr = bof3::addr;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch
// (0x903850.., Scratch_Swap at +0xC; words or dwords by function, MAGIC225's
// burst colours at +0x10..+0x1B) and the four SVECTORs of Prim_VertexScratch
// (0x9037A0..0x9037BF). Both are read again after every call, as the
// originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};
constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};
// kFrameCounter: now Frame_Counter (symbols.toml). Frame_Counter, read as a byte

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }
void AddLong(unsigned char* p, std::uint32_t v) {
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) + v));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }
// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
unsigned char* PoolRecord(std::uint32_t pool, unsigned index) { return Mem(pool + index * 0x84u); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions, and other units' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using TaskFn = void (__cdecl*)(unsigned char*);
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }
unsigned Alloc(std::uint32_t address) { return MH_WORD(ByteFn, address)() & 0xFFu; }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

// Other units' functions (docs/magic_s38.md section 3) are ours now and
// called by name: S37's MagicFx_FreeCurrentRecord (a tail jmp) and
// MagicFx_PushRecordMatrix, S35's MagicFx_WaitOwnerChildren.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// A pool walked: every record with bit 0 becomes Sprite_Current, its +0x80
// the owner, for the record's task; both put back after each. The task and
// owner are read after the phase call that precedes the walk.
void WalkPool(std::uint32_t pool, unsigned records, std::uint32_t task) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}
// The first `records` of a pool without bit 0 of +0 get it; the index, 0xFF
// when all are taken.
unsigned char PoolAlloc(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}
void ClearPool(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// A child task (kind 1): +0x80 this task, +1 0, counted in this task's +0xB.
// This task is read after BattleTask_Create; the slot is written unchecked
// (0xFF, none free, lands past the image, as in the original).
void AdoptChild(unsigned slot) {
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 0;
    Inc(self[0xB]);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to `slot`.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S38_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC223 (row 137, Tempest and Hurricane read one id down)

namespace {
// kGustAngles: now TempestGust_Angles (symbols.toml). TempestGust_Angles: four dwords by direction
}  // namespace

// ===========================================================================
// MAGIC225 (row 140, an id with no label read one id down)

namespace {

// kBurstColours: now Magic225Veil_BurstColours (symbols.toml). Magic225Veil_BurstColours: four RGB byte triples

}  // namespace

// ===========================================================================
// MAGIC226/227 (rows 135 and 147, MeteorStrike read one id down)

namespace {
// kChipShades: now MeteorStrikeChip_Shades (symbols.toml). MeteorStrikeChip_Shades: four bytes by i / 8
// kChipFades: now MeteorStrikeChip_Fades (symbols.toml). MeteorStrikeChip_Fades: four bytes by i / 8
// kChipLifts: now MeteorStrikeChip_Lifts (symbols.toml). MeteorStrikeChip_Lifts: four dwords by +0xB & 3
}  // namespace

void MagicS38_Inject() {
    if (bof3::WantsShadow("magic_s38")) magic_s38::SelfTest();
}
