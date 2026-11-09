// Group S21 of the spell round: the PSX's MAGIC093, MAGIC094 and MAGIC095
// overlays compiled into the exe (Magic_Rows rows 103, 20 and 22; the
// abilities that load them, read one id down, are Inferno, Frost and
// Iceblast - labels, not a measurement). docs/magic_s21.md.
//
//   MAGIC093 0x4C6300..0x4C742A  the flame effect: its task, eight scorch
//            children on the targets (kind 1, 0x4D), and a pool of 32 flame
//            columns and sparks at 0x6948D8 that its task runs itself;
//   MAGIC094 0x4C7430..0x4C7CCE  the frost effect: its task and a kind-1
//            child (0x15) that draws six rings of shards and a disc;
//   MAGIC095 0x4C7CD0..0x4C8D34  the ice effect: its task, which draws two
//            sets of spires and a disc, and twelve kind-1 shards (0x17).
//
// The kind-1 children and the pool's phases are reached from more than their
// own overlay (tools/magic_rows.py: 088, 091 and 092 create the same
// kinds); taking them here takes them for every overlay that reaches them.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), and a phase
// a .data table holds is called through the cell, read in place, so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past one
// of the three stack tables aborts where the original would call through its
// own stack (docs/magic_fx_reached.md section 3, the precedent), and a centre
// taken over no live actor aborts where the original divides by zero
// (Inferno_TargetCentre).
#include "game/magic_s21.h"

#include <cstdint>
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
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// --- what the three overlays share -----------------------------------------------

// kJitter: now Iceblast_Jitter (symbols.toml).

// The .data handler tables, read in place by their dispatchers (index
// unchecked, as the originals: a byte past the table reads the next one).
// kScorchPhases: now FxScorch_Phases (symbols.toml). FxScorch_Dispatch, by +1: one entry
// kScorchSteps: now FxScorch_Steps (symbols.toml). FxScorch_Run, by +2: three
// kPoolTypes: now FlamePool_Types (symbols.toml). FlamePool_Dispatch, by +1: two
// kColumnPhases: now FlameColumn_Phases (symbols.toml). FlameColumn_Task, by +2: four
// kSparkPhases: now FlameSpark_Phases (symbols.toml). FlameSpark_Task, by +2: four
// kRingPhases: now FrostRing_Phases (symbols.toml). FrostRing_Task, by +1: four
// kShardPhases: now IceShard_Phases (symbols.toml). IceShard_Task, by +1: two

// The PC's copy of the PSX scratchpad (SharedScratch) and Prim_VertexScratch:
// four SVECTORs 8 bytes apart, A0 / A8 / B0 / B8.
constexpr ::bof3::image::Va kScratch{SharedScratch_addr};
// kVertex: now Prim_VertexScratch (symbols.toml).
unsigned char* ScrMagicS21(unsigned off) { return Mem(::bof3::Img(kScratch.v) + off); }
unsigned char* Vtx(unsigned off) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + off); }
const short* VtxP(unsigned off) { return reinterpret_cast<const short*>(Vtx(off)); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
std::int32_t L(std::uint32_t address) { return Long(Mem(address)); }

// imul r32, r/m32: the low 32 bits, then an arithmetic shift.
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
void BumpMagicS21(unsigned char& b, int d = 1) { b = static_cast<unsigned char>(b + d); }
unsigned char U8(int v) { return static_cast<unsigned char>(v); }
float* F(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }
void PutF(unsigned char* prim, unsigned off, std::int32_t v) { *F(prim, off) = static_cast<float>(v); }
std::uint32_t Addr(const void* p) { return Low32(p); }

// An actor's record as the originals index them: 0..2 the party (stride
// 0x14C), else an enemy (index - 3, stride 0x128), none of it checked.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }
// The target's record by its byte: the original reads the byte, then the
// dword at 0x904B44 again and indexes by its low byte.
unsigned char* TargetRecord() {
    const unsigned char t = Mem(::bof3::Img(at::kTarget))[0];
    const unsigned low = static_cast<std::uint32_t>(L(::bof3::Img(at::kTarget))) & 0xFF;
    return t < 3 ? PartyRecord(low) : EnemyRecord(low - 3);
}

void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(L(table + 4 * index))))();
}

// The link of a primitive at the task's position: MapView_LinkPrimAt(+0x34,
// +0x38, slot, size), Sprite_Current read once.
void LinkAtTask(int slot, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                slot, size);
}

}  // namespace

#define MS21_EXPORT extern "C" __attribute__((disable_tail_calls))

MS21_EXPORT unsigned char __cdecl FlamePool_Alloc(void);
MS21_EXPORT void __cdecl FlamePool_Dispatch(void);
MS21_EXPORT void __cdecl Inferno_TargetCentre(void);
MS21_EXPORT void __cdecl FlameColumn_DrawDisc(void);
MS21_EXPORT void __cdecl FlameColumn_DrawBand(void);
MS21_EXPORT void __cdecl FlameSpark_Draw(void);
MS21_EXPORT void __cdecl FrostRing_PushMatrix(void);
MS21_EXPORT void __cdecl FrostRing_DrawShards(void);
MS21_EXPORT void __cdecl FrostRing_DrawDisc(void);
MS21_EXPORT void __cdecl Iceblast_DrawSpires(void);
MS21_EXPORT void __cdecl Iceblast_DrawSpiresInner(void);
MS21_EXPORT void __cdecl Iceblast_DrawDisc(void);
MS21_EXPORT void __cdecl IceShard_Draw(void);

// ===========================================================================
// MAGIC093: the flame effect

// --- the scorch child (kind 1, parameter 0x4D) ---------------------------------

// --- the pool: flame columns and sparks -----------------------------------------

// ===========================================================================
// MAGIC094: the frost effect

// --- the ring child (kind 1, parameter 0x15) -------------------------------------

// ===========================================================================
// MAGIC095: the ice effect

// --- the shard child (kind 1, parameter 0x17) -------------------------------------

void MagicS21_Inject() {
    if (bof3::WantsShadow("magic_s21")) magic_s21::SelfTest();
}
