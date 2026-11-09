// Five spell overlays compiled into the exe, round nine group S10
// (docs/magic_s10.md): the PSX's MAGIC052, MAGIC053, MAGIC054, MAGIC055 and
// MAGIC056.EMI, Magic_Rows rows 107, 134, 56, 65 and 112. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Ovum, Lavaburst,
// Howling, Ebonfire and Sacrifice; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC052 0x4ABCA0..0x4ABF16: children (kind 1, 0x50) near the source
//     sprite, one every eighth frame, each playing animation 5 with a sound
//     and freeing itself;
//   - MAGIC053 0x4ABF20..0x4ACAA6: eight children (kind 1, 0x5E) that fly up
//     round the side's centre, shake the camera and fill a pool of 32
//     records of its own (0x67F700), each a textured quad that spreads and
//     fades; the children draw a glow of 64 triangles;
//   - MAGIC054 0x4ACAB0..0x4ACFDA: every live actor of the target's side
//     copied into a child (kind 1, 0x2C) that plays its animation and scales
//     out and back; in an event battle whose byte 0x904AAA is 0x37, only
//     the target flags and a jump to the table's last step;
//   - MAGIC055 0x4ACFE0..0x4AD892: the disc-and-fan effect (FxDiscFan_Task's
//     twin) with its own start and six rings of its own draw;
//   - MAGIC056 0x4AD8A0..0x4AE7B6: the acting actor copied into a child that
//     darkens, splits off a ring (eight bands of 32 quads) and a disc (16
//     triangles), and comes back.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s10.h"

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
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
// kEventBattle: now EventBattle_Id (symbols.toml). u8: not 0 in an event battle (its kind)
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

// kChildDelays: now Lavaburst_ChildDelays (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

std::uint16_t SW(unsigned k) { return move_script::Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return move_script::Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }
void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }
// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the party
// member's by index, the enemy's by index - 3 (a party index lands below the
// enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// A task's copy of a record's first 0x80 bytes (`rep movsd`: dword by dword,
// forward).
void CopyRecord(unsigned char* to, const unsigned char* from) {
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(to + k, Long(from + k));
}

// This group's functions, and other groups' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using AnimFn = void (__cdecl*)(std::uint32_t, std::uint32_t);
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// The phase handlers and callees of other units (docs/magic_s10.md section 3)
// are ours now and called by name: S38's MagicFx_CountDownFlag10 and
// MeteorStrikeRock_DrawRing, S11's MagicFx_UncountAndFree, S37's
// MagicFx_PushRecordMatrix, MagicFx_FreeCurrentRecord and CombustionMote_Start.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// The callees with the arguments the originals push: both projections get a
// flag pointer past the depth, which ours does not read.
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S10_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Prim_VertexScratch's first three SVECTORs projected to +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S10_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}
// All four projected to +8, +8 + step, +8 + 2 step, +8 + 3 step.
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S10_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

}  // namespace

#define S10_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC052 (row 107, Ovum read one id down)

// ===========================================================================
// MAGIC053 (row 134, Lavaburst read one id down)

// ===========================================================================
// MAGIC054 (row 56, Howling read one id down)

// ===========================================================================
// MAGIC055 (row 65, Ebonfire read one id down)

// ===========================================================================
// MAGIC056 (row 112, Sacrifice read one id down)

void MagicS10_Inject() {
    if (bof3::WantsShadow("magic_s10")) magic_s10::SelfTest();
}
