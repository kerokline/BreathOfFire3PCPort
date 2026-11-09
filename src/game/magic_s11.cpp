// Two spell overlays compiled into the exe, round nine group S11
// (docs/magic_s11.md): the PSX's MAGIC058 and MAGIC059.EMI, Magic_Rows rows 82
// and 83. Read one id down (docs/cut-content.md section 2) the sibling labels
// them Sanctuary and Tornado; the names below use those labels as hypotheses,
// and say what the code does.
//
//   - MAGIC058 0x4AF040..0x4AFB76: one child (kind 1, 0x3F), a flat ring of 64
//     gouraud quads round the caster that widens; each live actor it reaches
//     (0x4F5970, MAGIC218's range test) but the acting one gets two motes from
//     the effect's own pool of 36 at 0x680780, which drop onto the actor and
//     draw a smaller ring of 16 quads as they fade; then a battle message
//     (Msg_SystemPtr 0x14) and the end;
//   - MAGIC059 0x4AFB80..0x4B0D42: a column over the caster - one child (kind
//     1, 0x40) drawing sixteen bands of a turning funnel, 24 motes from the
//     effect's own pool of 24 at 0x681A20 in three rings, each an orbiting
//     textured quad - while the effect's task circles about a point beside the
//     caster at the height of the opposite side's average.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. Calls into functions other groups own go by
// name (MAGIC218's AuraBreath_InReach 0x4F5970, MAGIC219's
// MagicFx_FreeCurrentRecord 0x4F6290), the engine's 0x446770 by raw address.
//
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through its own stack
// (docs/magic_fx_reached.md section 3, the precedent), and
// Tornado_AverageHeight aborts where the original divides by a count of 0
// live actors (the owner's call, round9 doc section 6). The .data tables are
// read in place, their indexes unchecked, as the originals read them.
#include "game/magic_s11.h"

#include <cstdint>
#include <cstring>

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
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words by function)
// and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are
// read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// kFacingPhase: now Tornado_FacingPhase (symbols.toml). Tornado_FacingPhase: a u8 in each of four dwords, by +8

constexpr std::uint32_t kPoolStride = 0x84;

// The .data dispatch tables, read in place.
// kRingTypes: now SanctuaryRing_Types (symbols.toml). SanctuaryRing_Types: 1 entry by +1
// kSMoteTypes: now SanctuaryMote_Types (symbols.toml). SanctuaryMote_Types: 1 entry by +1
// kFunnelTypes: now TornadoFunnel_Types (symbols.toml). TornadoFunnel_Types: 1 entry by +1
// kFunnelPhases: now TornadoFunnel_Phases (symbols.toml). TornadoFunnel_Phases: 4 entries by +2
// kTMoteTypes: now TornadoMote_Types (symbols.toml). TornadoMote_Types: 1 entry by +1
// kTMotePhases: now TornadoMote_Phases (symbols.toml). TornadoMote_Phases: 4 entries by +2

using Fn0 = void (__cdecl*)();

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
std::uint32_t Key(const void* p) { return Low32(p); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `imul` then `sar n`: the 32-bit product wraps, the shift is arithmetic.
int MulSar(int a, int b, unsigned n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}
int Mul12(int a, int b) { return MulSar(a, b, 12); }
int Add(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
// The originals index the enemies by the battle index - 3.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0).
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// Gte_RotTransPers4 with the ten arguments the originals push: the four
// SVECTORs of the vertex scratch, the primitive's four points, and the depth
// and flag cells of the caller's frame.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S11_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))
void Rtp4(unsigned char* prim) {
    long p, flag;
    S11_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &flag, &p);
}

// The target's done flag, 0x904AA8 bit 2, and the task freed.
void MarkDoneAndFree() {
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// A pool run by the effect's own task (Sanctuary_Task, Tornado_Task): every
// live record (+0 bit 0) as Sprite_Current, 0x93B940 its +0x80, for the call;
// both put back to what they were before the loop after each.
void RunPool(std::uint32_t pool, unsigned count, std::uint32_t dispatch) {
    unsigned char* const saved = Sc();
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kPoolStride);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t its = Long(r + 0x80);
        Sprite_Current = r;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        Call0(dispatch);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = saved;
    }
}

// The first free record of a pool (+0 bit 0 clear) marked in use; its index
// in al, or 0xFF when all are.
unsigned char PoolAlloc(std::uint32_t pool, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kPoolStride);
        if ((r[0] & 1) == 0) {
            r[0] = static_cast<unsigned char>(r[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

}  // namespace

// Every function here is called by Capcom's code through the jmp Inject
// writes at its address: extern "C" and no tail calls (a task's top frame).
#define S11_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC058 (row 82, Sanctuary read one id down)

namespace {

void Shade(unsigned char* p, unsigned vertex, unsigned char c) {
    p[4 + vertex * 0x10] = c;
    p[5 + vertex * 0x10] = c;
    p[6 + vertex * 0x10] = c;
}

}  // namespace

// ===========================================================================
// MAGIC059 (row 83, Tornado read one id down)

#undef S11_EXPORT

void MagicS11_Inject() {
    if (bof3::WantsShadow("magic_s11")) magic_s11::SelfTest();
}
