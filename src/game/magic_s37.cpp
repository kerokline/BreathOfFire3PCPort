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
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
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
// kV: now Prim_VertexScratch (symbols.toml).

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};
constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

// kMagmaDelays: now MagmaBreath_SpawnDelays (symbols.toml). MagmaBreath_SpawnDelays

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

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
std::uint32_t UMagicS37(std::int32_t v) { return static_cast<std::uint32_t>(v); }
std::uint32_t Key(const void* p) { return Low32(p); }
void CopyLong(unsigned char* to, const unsigned char* from) { SetLong(to, LongMagicS37(from)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }


std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
void Sound(unsigned id) { MH_CALL(Sound_PlayById)(static_cast<unsigned short>(id)); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// The phase handlers of MAGIC226/227 a table holds (S38's
// MagicFx_CountDownFlag10 and MeteorStrikeRock_Hide) are ours now and named
// in the tables.

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

// One MATRIX block as the originals lay it out on their stack: the rotation,
// then the translation RotTrans writes at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC219 (row 138, Magma Breath read one id down)

// ===========================================================================
// MAGIC220 / MAGIC221 (rows 136 and 146, Geo Breath and Gaea's Breath read
// one id down: one code)

// ===========================================================================
// MAGIC222 (row 133, Combustion read one id down)

void MagicS37_Inject() {
    if (bof3::WantsShadow("magic_s37")) magic_s37::SelfTest();
}
