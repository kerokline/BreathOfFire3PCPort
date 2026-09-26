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

// The pool of flame columns and sparks MAGIC093's task runs (32 of the task
// slots' 0x84 bytes: +0 bit 0 in use, +1 the kind, +0x80 the owner), and the
// 56 signed jitter bytes MAGIC095's start fills, straight after it.
constexpr std::uint32_t kPool = 0x6948D8;
constexpr unsigned kPoolSlots = 32;
constexpr std::uint32_t kJitter = 0x695958;

// The .data handler tables, read in place by their dispatchers (index
// unchecked, as the originals: a byte past the table reads the next one).
constexpr std::uint32_t kScorchPhases = 0x65B604;     // FxScorch_Dispatch, by +1: one entry
constexpr std::uint32_t kScorchSteps = 0x65B608;      // FxScorch_Run, by +2: three
constexpr std::uint32_t kPoolTypes = 0x65B614;        // FlamePool_Dispatch, by +1: two
constexpr std::uint32_t kColumnPhases = 0x65B61C;     // FlameColumn_Task, by +2: four
constexpr std::uint32_t kSparkPhases = 0x65B62C;      // FlameSpark_Task, by +2: four
constexpr std::uint32_t kRingPhases = 0x65B63C;       // FrostRing_Task, by +1: four
constexpr std::uint32_t kShardPhases = 0x65B64C;      // IceShard_Task, by +1: two

// The PC's copy of the PSX scratchpad (DamageScratch) and Prim_VertexScratch:
// four SVECTORs 8 bytes apart, A0 / A8 / B0 / B8.
constexpr std::uint32_t kScratch = 0x903850;
constexpr std::uint32_t kVertex = 0x9037A0;
unsigned char* Scr(unsigned off) { return Mem(kScratch + off); }
unsigned char* Vtx(unsigned off) { return Mem(kVertex + off); }
const short* VtxP(unsigned off) { return reinterpret_cast<const short*>(Vtx(off)); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
std::int32_t L(std::uint32_t address) { return Long(Mem(address)); }

// A CLUT strip's two copies: the live one at 0x812980 and the buffer 0x4000
// below it (docs/magic_fx_reached.md section 4, FxDiscFan_Start's).
constexpr std::uint32_t kClut = 0x812980;
constexpr std::uint32_t kClutEnd = 0x812B80;

// Unnamed callees, called by their addresses, never bound:
//   0x4F6290  a pool slot's free (bytes +0..+4 of Sprite_Current cleared) -
//             MAGIC219's (group S37), shared by 35 overlays;
//   0x5A76F0  a POLY_G4-shaped setter, code 0x5C, the four z at +0x10 ..;
//   0x5A7570  POLY_F3's setter, code 0x20, the three z at +0x10, +0x1C, +0x28.
constexpr std::uint32_t kSlotFree = 0x4F6290;
constexpr std::uint32_t kSetPoly5C = 0x5A76F0;
constexpr std::uint32_t kSetPolyF3 = 0x5A7570;
using VoidFn = void (__cdecl*)();
using PrimFn = void (__cdecl*)(unsigned char*);

// imul r32, r/m32: the low 32 bits, then an arithmetic shift.
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
void Bump(unsigned char& b, int d = 1) { b = static_cast<unsigned char>(b + d); }
unsigned char U8(int v) { return static_cast<unsigned char>(v); }
float* F(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }
void PutF(unsigned char* prim, unsigned off, std::int32_t v) { *F(prim, off) = static_cast<float>(v); }
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// An actor's record as the originals index them: 0..2 the party (stride
// 0x14C), else an enemy (index - 3, stride 0x128), none of it checked.
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned i) { return Mem(at::kEnemies + i * at::kEnemyStride); }
// The target's record by its byte: the original reads the byte, then the
// dword at 0x904B44 again and indexes by its low byte.
unsigned char* TargetRecord() {
    const unsigned char t = Mem(at::kTarget)[0];
    const unsigned low = static_cast<std::uint32_t>(L(at::kTarget)) & 0xFF;
    return t < 3 ? PartyRecord(low) : EnemyRecord(low - 3);
}

void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(L(table + 4 * index))))();
}

// The link of a primitive at the task's position: MapView_LinkPrimAt(+0x34,
// +0x38, slot, size), Sprite_Current read once.
void LinkAtTask(int slot, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)),
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

// original 0x4C6300 (Magic_Rows row 103): the kind-2 task. Its phase +1
// through a two-entry stack table - Inferno_Start, Inferno_End - then every
// pool slot with bit 0 run through FlamePool_Dispatch as Sprite_Current, its
// +0x80 the owner, both put back after each. The task and the owner are read
// after the phase, once. The index is not checked by the original: ours
// aborts past 1.
MS21_EXPORT void __cdecl Inferno_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Inferno_Start, bof3::addr::Inferno_End};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Inferno_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const task = Sprite_Current;
    const std::int32_t owner = L(at::kOwner);
    for (unsigned i = 0; i < kPoolSlots; ++i) {
        unsigned char* const slot = Mem(kPool + i * at::kTaskStride);
        if ((slot[0] & 1) == 0) continue;
        const std::int32_t its = Long(slot + 0x80);
        Sprite_Current = slot;
        SetLong(Mem(at::kOwner), its);
        MH_CALL(FlamePool_Dispatch)();
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = task;
    }
}

// original 0x4C6380: bytes +0..+2 of every pool slot cleared; the task to
// the targets' centre; +0xB and +9 0, the phase on. Then a scorch child
// (kind 1, 0x4D: +0x80 the task, +1 0, +3 the index, +9 0x10) on every
// target not out - the eight enemies when the target byte has bit 0x40, else
// the three party members - counted in the task's +9; eight flame columns
// (pool kind 0, +4 the index) and sixteen sparks (kind 1, +0xB the index, +9
// (index + 3) * 8), counted in +0xB; the CLUT strip made semi-transparent
// (every word of the buffer 0x4000 below with bit 15, into 0x812980..);
// Gfx_ClutStripDirty; sound 0x100. The pool's answer is used unchecked (0xFF
// with every slot in use: a write past the pool).
MS21_EXPORT void __cdecl Inferno_Start(void) {
    for (std::uint32_t p = kPool + 1; p < kPool + kPoolSlots * at::kTaskStride + 1; p += at::kTaskStride) {
        Mem(p)[-1] = 0;
        Mem(p)[0] = 0;
        Mem(p)[1] = 0;
    }
    MH_CALL(Inferno_TargetCentre)();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    Bump(Sprite_Current[1]);
    const bool enemies = (Mem(at::kTarget)[0] & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned k = MH_CALL(BattleTask_Create)(1, 0x4D);
        unsigned char* const child = Mem(at::kTasks + k * at::kTaskStride);
        unsigned char* const sc = Sprite_Current;
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(sc)));
        child[1] = 0;
        child[3] = U8(i);
        child[9] = 0x10;
        Bump(sc[9]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned k = MH_CALL(FlamePool_Alloc)();
        unsigned char* const slot = Mem(kPool + k * at::kTaskStride);
        unsigned char* const sc = Sprite_Current;
        SetLong(slot + 0x80, static_cast<std::int32_t>(Addr(sc)));
        slot[1] = 0;
        slot[4] = U8(i);
        Bump(sc[0xB]);
    }
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned k = MH_CALL(FlamePool_Alloc)();
        unsigned char* const slot = Mem(kPool + k * at::kTaskStride);
        unsigned char* const sc = Sprite_Current;
        SetLong(slot + 0x80, static_cast<std::int32_t>(Addr(sc)));
        slot[1] = 1;
        slot[0xB] = U8(i);
        slot[9] = U8((i + 3) << 3);
        Bump(sc[0xB]);
    }
    for (std::uint32_t p = kClut; p < kClutEnd; p += 2) SetWord(Mem(p), Word(Mem(p - 0x4000)) | 0x8000u);
    Mem(0x937F90)[0] = 1;   // Gfx_ClutStripDirty
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C6540: held while the task's +0xB (flames) or +9 (scorches)
// counts; then the done flag and free. Sprite_Current read once.
MS21_EXPORT void __cdecl Inferno_End(void) {
    const unsigned char* const sc = Sprite_Current;
    if (sc[0xB] != 0 || sc[9] != 0) return;
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C72C0: the first
// pool slot without bit 0, marked and its index answered; 0xFF when all 32
// are in use. Only al is the original's answer; every caller keeps al.
MS21_EXPORT unsigned char __cdecl FlamePool_Alloc(void) {
    unsigned char i = 0;
    do {
        unsigned char* const s = Mem(kPool + i * at::kTaskStride);
        if ((s[0] & 1) == 0) {
            s[0] = static_cast<unsigned char>(s[0] | 1);
            return i;
        }
        ++i;
    } while (i < kPoolSlots);
    return 0xFF;
}

// original 0x4C7320: the task's position to the centre of the targets not
// out - the eight enemies with the target byte's bit 0x40, else the three
// party members: +0x34 / +0x38 the mean of (x >> 9) - 0x4000 put back
// (+ 0x4000) << 9, the word +0x3E the mean height (+0x3E), idiv each. With
// every one out the original divides by zero (a #DE, the game's end); ours
// aborts with a Fatal.
MS21_EXPORT void __cdecl Inferno_TargetCentre(void) {
    int n = 0, x = 0, z = 0, y = 0;
    const bool enemies = (Mem(at::kTarget)[0] & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned char* const r = enemies ? EnemyRecord(i) : PartyRecord(i);
        x += (Long(r + 0x34) >> 9) - 0x4000;
        y += S16(r + 0x3E);
        z += (Long(r + 0x38) >> 9) - 0x4000;
        ++n;
    }
    if (n == 0) bof3::Fatal("Inferno_TargetCentre: every target is out (the original divides by zero)");
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(x / n + 0x4000) << 9));
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(z / n + 0x4000) << 9));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(y / n));
}

// --- the scorch child (kind 1, parameter 0x4D) ---------------------------------

// original 0x4C6560: jmp [0x65B604 + 4 * +1] - one entry, FxScorch_Run.
MS21_EXPORT void __cdecl FxScorch_Dispatch(void) { CallCell(kScorchPhases, Sprite_Current[1]); }

// original 0x4C6580: jmp [0x65B608 + 4 * +2] - Delay, Hit, Free.
MS21_EXPORT void __cdecl FxScorch_Run(void) { CallCell(kScorchSteps, Sprite_Current[2]); }

// original 0x4C65A0: +9 down; at 0 the record of its actor (+3: an enemy's
// index with the target byte's bit 0x40, else a party member's) has its tint
// released and set to (-8, -8, -8, 1), the actor's flags 0x10; the step on.
MS21_EXPORT void __cdecl FxScorch_Delay(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    if (Mem(at::kTarget)[0] & 0x40) {
        unsigned char* const r = EnemyRecord(sc[3]);
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 0xF8, 0xF8, 0xF8, 1);
        MH_CALL(Battle_SetTargetFlags)(static_cast<unsigned char>(Sprite_Current[3] + 3), 0x10);
    } else {
        unsigned char* const r = PartyRecord(sc[3]);
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 0xF8, 0xF8, 0xF8, 1);
        MH_CALL(Battle_SetTargetFlags)(Sprite_Current[3], 0x10);
    }
    Bump(Sprite_Current[2]);
}

// original 0x4C6650: held while the owner's +0xB (its flames) is 8 or more;
// then the actor's flag 0x40, its tint released, it flashed; the step on.
MS21_EXPORT void __cdecl FxScorch_Hit(void) {
    if (Pointer(at::kOwner)[0xB] >= 8) return;
    const bool enemies = (Mem(at::kTarget)[0] & 0x40) != 0;
    const unsigned char i = Sprite_Current[3];
    if (enemies) {
        MH_CALL(Battle_SetTargetFlag40)(static_cast<unsigned char>(i + 3));
        MH_CALL(Sprite_ReleaseTint)(EnemyRecord(Sprite_Current[3]));
        MH_CALL(BattleActor_Flash)(static_cast<unsigned char>(Sprite_Current[3] + 3));
    } else {
        MH_CALL(Battle_SetTargetFlag40)(i);
        MH_CALL(Sprite_ReleaseTint)(PartyRecord(Sprite_Current[3]));
        MH_CALL(BattleActor_Flash)(Sprite_Current[3]);
    }
    Bump(Sprite_Current[2]);
}

// original 0x4C66F0: the owner's +9 (its scorches) down; free.
MS21_EXPORT void __cdecl FxScorch_Free(void) {
    Bump(Pointer(at::kOwner)[9], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// --- the pool: flame columns and sparks -----------------------------------------

// original 0x4C6700: jmp [0x65B614 + 4 * +1] - FlameColumn_Task,
// FlameSpark_Task.
MS21_EXPORT void __cdecl FlamePool_Dispatch(void) { CallCell(kPoolTypes, Sprite_Current[1]); }

// original 0x4C6720: the column's step through [0x65B61C + 4 * +2], its +0xB
// (the flicker) up; while it lives past its first step, the disc and the
// band under the actor's matrix.
MS21_EXPORT void __cdecl FlameColumn_Task(void) {
    CallCell(kColumnPhases, Sprite_Current[2]);
    Bump(Sprite_Current[0xB]);
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(FlameColumn_DrawDisc)();
    MH_CALL(FlameColumn_DrawBand)();
    MH_CALL(Gte_PopMatrix)();
}

// The column's place on its circle: radius word 0x903850 (x 1/8), angle word
// 0x90385A = (+4 & 7) << 9, round the owner.
static void ColumnPlace() {
    const unsigned char* sc = Sprite_Current;
    SetWord(Scr(0xA), static_cast<unsigned>((sc[4] & 7) << 9));
    const int s = MH_CALL(Math_Sin)(S16(Scr(0xA)));
    SetLong(Sprite_Current + 0x34, (Mul(s, S16(Scr(0))) >> 3) + Long(Pointer(at::kOwner) + 0x34));
    const int c = MH_CALL(Math_Cos)(S16(Scr(0xA)));
    SetLong(Sprite_Current + 0x38, (Mul(c, S16(Scr(0))) >> 3) + Long(Pointer(at::kOwner) + 0x38));
}

// original 0x4C6770: +0xA = (Rand & 3) << 4; the radius 0xC0 - +0xA; placed
// on its circle round the owner, its height the owner's; +0xB = Rand; +9 0;
// the step on.
MS21_EXPORT void __cdecl FlameColumn_Start(void) {
    const int r = MH_CALL(Rand)();
    Sprite_Current[0xA] = U8((r & 3) << 4);
    SetWord(Scr(0), 0xC0u - Sprite_Current[0xA]);
    ColumnPlace();
    SetLong(Sprite_Current + 0x3C, Long(Pointer(at::kOwner) + 0x3C));
    const int r2 = MH_CALL(Rand)();
    Sprite_Current[0xB] = U8(r2);
    Sprite_Current[9] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4C6840: the radius +9 * 8 + 0xC0 - +0xA, placed again; +9 up,
// at 0x10 the step on.
MS21_EXPORT void __cdecl FlameColumn_Grow(void) {
    const unsigned char* const sc = Sprite_Current;
    SetWord(Scr(0), sc[9] * 8u + 0xC0u - sc[0xA]);
    ColumnPlace();
    Bump(Sprite_Current[9]);
    unsigned char* const s2 = Sprite_Current;
    if (s2[9] == 0x10) Bump(s2[2]);
}

// original 0x4C68F0: held until the owner's +0xB (its flames) is 8.
MS21_EXPORT void __cdecl FlameColumn_Hold(void) {
    if (Pointer(at::kOwner)[0xB] == 8) Bump(Sprite_Current[2]);
}

// original 0x4C6910: +9 down; at 0 the owner's +0xB down and the slot freed.
MS21_EXPORT void __cdecl FlameColumn_Fade(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    Bump(Pointer(at::kOwner)[0xB], -1);
    MH_AT(VoidFn, kSlotFree)();
}

// original 0x4C6940: a fan of sixteen gouraud triangles round the origin of
// the actor's matrix, radius 0x100 + sin((+0xB & 0x1F) << 7) >> 8; the
// centre (+9 * 10, +9 * 9, +9 * 2), the rim (+9 * 10, +9 * 4, +9 * 2), every
// channel a byte; each linked at the column's position.
MS21_EXPORT void __cdecl FlameColumn_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtTask(2, 0xC);
    const int s = MH_CALL(Math_Sin)((Sprite_Current[0xB] & 0x1F) << 7);
    const unsigned char b = Sprite_Current[9];
    SetWord(Scr(0), static_cast<unsigned>((Mul(s, 16) >> 12) + 0x100));
    const unsigned char red = U8(b * 10), g0 = U8(b * 9), g1 = U8(b << 2), blue = U8(b << 1);
    SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), S16(Scr(0))) >> 12));
    SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), S16(Scr(0))) >> 12));
    SetWord(Vtx(0x14), 0);
    SetWord(Vtx(0xC), 0);
    SetWord(Vtx(4), 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = Word(Vtx(0x10)), y = Word(Vtx(0x12));
        SetWord(Vtx(0), 0);
        SetWord(Vtx(2), 0);
        SetWord(Vtx(8), x);
        SetWord(Vtx(0xA), y);
        SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), S16(Scr(0))) >> 12));
        SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a), S16(Scr(0))) >> 12));
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x18), F(p, 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[5] = g0;
        p[4] = red;
        p[6] = blue;
        p[0x14] = red;
        p[0x15] = g1;
        p[0x16] = blue;
        p[0x24] = red;
        p[0x25] = g1;
        p[0x26] = blue;
        LinkAtTask(2, 0x34);
    }
}

// original 0x4C6B10: a band of sixteen gouraud quads round the origin, the
// inner radius 0x180 + sin(..) >> 8 at A0 / A8, the outer 0x100 + .. at
// B0 / B8; inner vertices (1, 1, 1), outer (+9 * 10, +9 * 4, +9 * 2).
MS21_EXPORT void __cdecl FlameColumn_DrawBand(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtTask(2, 0xC);
    const int s = MH_CALL(Math_Sin)((Sprite_Current[0xB] & 0x1F) << 7);
    const unsigned char flicker = Sprite_Current[0xB];
    SetWord(Scr(0), static_cast<unsigned>((Mul(s, 16) >> 12) + 0x100));
    const int s2 = MH_CALL(Math_Sin)((flicker & 0x1F) << 7);
    const unsigned char b = Sprite_Current[9];
    SetWord(Scr(2), static_cast<unsigned>((Mul(s2, 16) >> 12) + 0x180));
    const unsigned char red = U8(b * 10), blue = U8(b << 1), green = U8(b << 2);
    SetWord(Vtx(8), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), S16(Scr(2))) >> 12));
    SetWord(Vtx(0xA), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), S16(Scr(2))) >> 12));
    SetWord(Vtx(0x18), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), S16(Scr(0))) >> 12));
    const int c0 = MH_CALL(Math_Cos)(0);
    SetWord(Vtx(0x1C), 0);
    SetWord(Vtx(0x1A), static_cast<unsigned>(Mul(c0, S16(Scr(0))) >> 12));
    SetWord(Vtx(0x14), 0);
    SetWord(Vtx(0xC), 0);
    SetWord(Vtx(4), 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = Word(Vtx(8)), y = Word(Vtx(0xA));
            SetWord(Vtx(0), x);
            SetWord(Vtx(2), y);
        }
        SetWord(Vtx(8), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), S16(Scr(2))) >> 12));
        const int c = MH_CALL(Math_Cos)(a);
        const std::uint16_t ox = Word(Vtx(0x18));
        SetWord(Vtx(0xA), static_cast<unsigned>(Mul(c, S16(Scr(2))) >> 12));
        const std::uint16_t oy = Word(Vtx(0x1A));
        SetWord(Vtx(0x10), ox);
        SetWord(Vtx(0x12), oy);
        SetWord(Vtx(0x18), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), S16(Scr(0))) >> 12));
        SetWord(Vtx(0x1A), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a), S16(Scr(0))) >> 12));
        long depth;
        MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), F(p, 8), F(p, 0x18), F(p, 0x28), F(p, 0x38),
                                   &depth);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = red;
        p[0x25] = green;
        p[0x26] = blue;
        p[0x34] = red;
        p[0x35] = green;
        p[0x36] = blue;
        LinkAtTask(2, 0x44);
    }
}

// original 0x4C6D80: the spark's step through [0x65B62C + 4 * +2]; while it
// lives past its first step, its screen point and its quads.
MS21_EXPORT void __cdecl FlameSpark_Task(void) {
    CallCell(kSparkPhases, Sprite_Current[2]);
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(FlameSpark_Draw)();
}

// original 0x4C6DB0: +9 (the delay Inferno_Start gave it) down; at 0 the
// owner's position, moved 0x20 out in a random eighth ((Rand & 7) << 9);
// with +0xB odd a sound by (+0xB >> 1) % 3 - 0x101, 0x102, 0x103; +0xB 0x10,
// +9 0, +0xA 0x40, the step on.
MS21_EXPORT void __cdecl FlameSpark_Start(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    SetLong(sc + 0x34, Long(Pointer(at::kOwner) + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Pointer(at::kOwner) + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(Pointer(at::kOwner) + 0x3C));
    SetWord(Scr(0), 0x100);
    const int r = MH_CALL(Rand)();
    unsigned char* const x = Sprite_Current + 0x34;
    const int angle = static_cast<short>(static_cast<std::uint16_t>((r & 7) << 9));
    SetWord(Scr(0xA), static_cast<unsigned>(angle));
    const int s = MH_CALL(Math_Sin)(angle);
    SetLong(x, Long(x) + (Mul(s, S16(Scr(0))) >> 3));
    unsigned char* const z = Sprite_Current + 0x38;
    const int c = MH_CALL(Math_Cos)(S16(Scr(0xA)));
    SetLong(z, Long(z) + (Mul(c, S16(Scr(0))) >> 3));
    const unsigned char tone = Sprite_Current[0xB];
    if (tone & 1) {
        const unsigned which = (tone >> 1) % 3;
        MH_CALL(Sound_PlayById)(which == 0 ? 0x101 : which == 1 ? 0x102 : 0x103);
    }
    Sprite_Current[0xB] = 0x10;
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0x40;
    Bump(Sprite_Current[2]);
}

// original 0x4C6ED0: +9 (the width) up by 0x20 to 0xC0.
MS21_EXPORT void __cdecl FlameSpark_Grow(void) {
    Bump(Sprite_Current[9], 0x20);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0xC0) Bump(sc[2]);
}

// original 0x4C6EF0: +9 up by 2, +0xA (the height) down to 0x30.
MS21_EXPORT void __cdecl FlameSpark_Stretch(void) {
    Bump(Sprite_Current[9], 2);
    Bump(Sprite_Current[0xA], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[0xA] == 0x30) Bump(sc[2]);
}

// original 0x4C6F20: +9 up, +0xA and +0xB (the shade) down; at +0xB 0 the
// owner's +0xB down and the slot freed.
MS21_EXPORT void __cdecl FlameSpark_Fade(void) {
    Bump(Sprite_Current[9]);
    Bump(Sprite_Current[0xA], -1);
    Bump(Sprite_Current[0xB], -1);
    if (Sprite_Current[0xB] != 0) return;
    Bump(Pointer(at::kOwner)[0xB], -1);
    MH_AT(VoidFn, kSlotFree)();
}

// original 0x4C6F70: two textured quads at the spark's screen point (+0x2E,
// +0x30), half-width +0xA, height +9, shade +0xB * 8: the flame (v 0x40 ..
// 0xFF of page (0x340, 0x100), clut (0, 0x1FA)) and its tip 16 above (v 0x30
// .. 0x40), the tip's lower two vertices (1, 1, 1). Scratch words 0x90385C
// (+9), 0x90385E (+0xA), 0x903856 / 0x903858 (the point), read again for
// every coordinate.
MS21_EXPORT void __cdecl FlameSpark_Draw(void) {
    const unsigned char* const sc = Sprite_Current;
    SetWord(Scr(0xC), sc[9]);
    const unsigned char shade = U8(sc[0xB] << 3);
    SetWord(Scr(0xE), sc[0xA]);
    SetWord(Scr(6), Word(sc + 0x2E));
    SetWord(Scr(8), Word(sc + 0x30));
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    LinkAtTask(0, 0xC);
    auto X = [] { return static_cast<int>(S16(Scr(6))); };
    auto Y = [] { return static_cast<int>(S16(Scr(8))); };
    auto H = [] { return static_cast<int>(S16(Scr(0xC))); };
    auto W = [] { return static_cast<int>(S16(Scr(0xE))); };
    unsigned char* p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutF(p, 8, X() - W());
    PutF(p, 0xC, Y() - H());
    PutF(p, 0x1C, X() + W());
    PutF(p, 0x20, Y() - H());
    PutF(p, 0x30, X() - W());
    PutF(p, 0x34, Y());
    PutF(p, 0x44, X() + W());
    PutF(p, 0x48, Y());
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au, 0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = shade;
    SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 0x40;
    p[0x15] = 0x40;
    p[0x28] = 0xC0;
    p[0x29] = 0x40;
    p[0x3C] = 0x40;
    p[0x3D] = 0xFF;
    p[0x50] = 0xC0;
    p[0x51] = 0xFF;
    LinkAtTask(0, 0x54);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutF(p, 8, X() - W());
    PutF(p, 0xC, Y() - H() - 0x10);
    PutF(p, 0x1C, X() + W());
    PutF(p, 0x20, Y() - H() - 0x10);
    PutF(p, 0x30, X() - W());
    PutF(p, 0x34, Y() - H());
    PutF(p, 0x44, X() + W());
    PutF(p, 0x48, Y() - H());
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = 1;
    for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = shade;
    SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 0x40;
    p[0x15] = 0x30;
    p[0x28] = 0xC0;
    p[0x29] = 0x30;
    p[0x3C] = 0x40;
    p[0x3D] = 0x40;
    p[0x50] = 0xC0;
    p[0x51] = 0x40;
    LinkAtTask(0, 0x54);
}

// ===========================================================================
// MAGIC094: the frost effect

// original 0x4C7430 (Magic_Rows row 20): the kind-2 task, its phase +1
// through a two-entry stack table - Frost_Start, Frost_Wait. Ours aborts
// past 1.
MS21_EXPORT void __cdecl Frost_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Frost_Start, bof3::addr::Frost_Wait};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Frost_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4C7460: the source sprite's (0x904B4C, read once) position and
// screen point (y 16 up) to the task; a ring child (kind 1, 0x15) owned by
// it with the source's actor byte and position; +0xB 0; the phase on; sound
// 0x100.
MS21_EXPORT void __cdecl Frost_Start(void) {
    unsigned char* const src = Pointer(at::kSource);
    SetLong(Sprite_Current + 0x34, Long(src + 0x34));
    SetLong(Sprite_Current + 0x38, Long(src + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(src + 0x3C));
    SetWord(Sprite_Current + 0x2E, Word(src + 0x2E));
    SetWord(Sprite_Current + 0x30, Word(src + 0x30) - 0x10u);
    const unsigned k = MH_CALL(BattleTask_Create)(1, 0x15);
    unsigned char* const child = Mem(at::kTasks + k * at::kTaskStride);
    unsigned char* const sc = Sprite_Current;
    SetLong(child + 0x80, static_cast<std::int32_t>(Addr(sc)));
    child[8] = src[8];
    SetLong(child + 0x34, Long(src + 0x34));
    SetLong(child + 0x38, Long(src + 0x38));
    SetLong(child + 0x3C, Long(src + 0x3C));
    sc[0xB] = 0;
    Bump(Sprite_Current[1]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C7510: the screen point from the source again; once the ring
// has set +0xB to 0xFF, the target's tint released, it flashed, its flag
// 0x40, the done flag, free.
MS21_EXPORT void __cdecl Frost_Wait(void) {
    const unsigned char* const src = Pointer(at::kSource);
    SetWord(Sprite_Current + 0x2E, Word(src + 0x2E));
    SetWord(Sprite_Current + 0x30, Word(src + 0x30) - 0x10u);
    if (Sprite_Current[0xB] != 0xFF) return;
    MH_CALL(Sprite_ReleaseTint)(TargetRecord());
    MH_CALL(BattleActor_Flash)(Mem(at::kTarget)[0]);
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// --- the ring child (kind 1, parameter 0x15) -------------------------------------

// original 0x4C75B0: its phase through [0x65B63C + 4 * +1]; unless the
// owner's +0xB is 0xFF, the shards and the disc under its own matrix; then
// always a draw mode (0x15) committed to slot 3.
MS21_EXPORT void __cdecl FrostRing_Task(void) {
    CallCell(kRingPhases, Sprite_Current[1]);
    if (Pointer(at::kOwner)[0xB] != 0xFF) {
        MH_CALL(FrostRing_PushMatrix)();
        MH_CALL(FrostRing_DrawShards)();
        MH_CALL(FrostRing_DrawDisc)();
        MH_CALL(Gte_PopMatrix)();
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4C7610: the target's flags 0x20; +9, +0xA 0; the phase on.
MS21_EXPORT void __cdecl FrostRing_Mark(void) {
    MH_CALL(Battle_SetTargetFlags)(Mem(at::kTarget)[0], 0x20);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4C7640: +0xA and +9 up; at +9 0x10 the target's tint released
// and set to (6, 4, 8, 0), +9 0, the phase on.
MS21_EXPORT void __cdecl FrostRing_Grow(void) {
    Bump(Sprite_Current[0xA]);
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x10) return;
    unsigned char* const r = TargetRecord();
    MH_CALL(Sprite_ReleaseTint)(r);
    MH_CALL(Sprite_SetTint)(r, 6, 4, 8, 0);
    Sprite_Current[9] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4C76D0: +0xA up to 0x10; +9 up, at 0x10 back to 0 and the phase
// on. The task is read again only after +0xA moves.
MS21_EXPORT void __cdecl FrostRing_Spin(void) {
    unsigned char* sc = Sprite_Current;
    if (sc[0xA] < 0x10) {
        Bump(sc[0xA]);
        sc = Sprite_Current;
    }
    Bump(sc[9]);
    unsigned char* const s2 = Sprite_Current;
    if (s2[9] != 0x10) return;
    s2[9] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4C7710: +9 up; at 0x20 the owner's +0xB 0xFF (Frost_Wait's
// signal) and free.
MS21_EXPORT void __cdecl FrostRing_Fade(void) {
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x20) return;
    Pointer(at::kOwner)[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C7740: six rings (i 0..5) of eight gouraud triangles each, one
// point on the axis (radius (i + 1) << 4) and two on a ring round it; the
// ring's radius and height by the phase +1 and +9:
//   phase 2: rim (i + 1) << 6, height -80 * (8 - (i ^ 7));
//   phase 3: rim ((i + 1) << 6) - +9, height 2 * (+9 - 40 * ((i ^ 7) + 1));
//   else:    rim 4 * +9 * (i + 1), height -5 * +9 * ((i ^ 7) + 1).
// The rim shade (r, r, 0xC0) with r = (i + 1) << 4 - in phase 3 less (+9 >> 1)
// * (i + 1), at least 1 when that goes negative, and blue 0xC1 - +9 * 6.
// Scratch dwords 0x903850 / 0x903854 / 0x903858 / 0x90385C, each read again.
MS21_EXPORT void __cdecl FrostRing_DrawShards(void) {
    int i = 0;
    do {
        const unsigned char* const sc = Sprite_Current;
        const int n = i + 1;
        const int r16 = n << 4;
        const int j = i ^ 7;
        if (sc[1] == 2) {
            SetLong(Scr(0), r16);
            SetLong(Scr(4), n << 6);
            SetLong(Scr(8), (-1 - j) * 80);
        } else if (sc[1] == 3) {
            SetLong(Scr(0), r16);
            SetLong(Scr(4), (n << 6) - sc[9]);
            SetLong(Scr(8), (sc[9] - (j + 1) * 40) * 2);
        } else {
            SetLong(Scr(0), r16);
            SetLong(Scr(4), Mul(sc[9], n) << 2);
            SetLong(Scr(8), -5 * Mul(j + 1, sc[9]));
        }
        SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), L(kScratch)) >> 12));
        SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), L(kScratch)) >> 12));
        SetWord(Vtx(0x14), 0);
        int a1 = 0x100;
        for (int a2 = 0x200; a2 < 0x1200; a2 += 0x200, a1 += 0x200) {
            SetWord(Vtx(0), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a1), L(kScratch + 4)) >> 12));
            const int s1 = MH_CALL(Math_Sin)(a1);
            const std::uint16_t h = Word(Scr(8)), px = Word(Vtx(0x10));
            SetWord(Vtx(2), static_cast<unsigned>(Mul(s1, L(kScratch + 4)) >> 12));
            const std::uint16_t pz = Word(Vtx(0x12));
            SetWord(Vtx(4), h);
            SetWord(Vtx(8), px);
            SetWord(Vtx(0xA), pz);
            SetWord(Vtx(0xC), 0);
            SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a2), L(kScratch)) >> 12));
            const int s2 = MH_CALL(Math_Sin)(a2);
            const int ox = S16(Vtx(0x10));
            const unsigned char* const s = Sprite_Current;
            SetWord(Vtx(0x14), 0);
            const std::int32_t oz = Mul(s2, L(kScratch)) >> 12;
            SetWord(Vtx(0x12), static_cast<unsigned>(oz));
            const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(ox) << 9) +
                                                               static_cast<std::uint32_t>(Long(s + 0x34)));
            const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(static_cast<short>(oz)) << 9) +
                                                               static_cast<std::uint32_t>(Long(s + 0x38)));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG3)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            const unsigned char* const s3 = Sprite_Current;
            if (s3[1] == 3) {
                SetLong(Scr(0xC), r16);
                const int less = -(r16 / 16) * (s3[9] >> 1);
                const std::int32_t v = L(kScratch + 0xC) + less;
                SetLong(Scr(0xC), v);
                if (v < 0) SetLong(Scr(0xC), 1);
                p[4] = 1;
                p[5] = 1;
                p[6] = 1;
                p[0x14] = Scr(0xC)[0];
                p[0x15] = Scr(0xC)[0];
                const unsigned char blue = U8(0xC1 - U8(Sprite_Current[9] * 6));
                p[0x16] = blue;
                p[0x24] = p[0x14];
                p[0x25] = p[0x15];
                p[0x26] = blue;
            } else {
                SetLong(Scr(0xC), r16);
                p[4] = 1;
                p[5] = 1;
                p[6] = 1;
                p[0x14] = Scr(0xC)[0];
                p[0x15] = Scr(0xC)[0];
                p[0x16] = 0xC0;
                p[0x24] = Scr(0xC)[0];
                p[0x25] = Scr(0xC)[0];
                p[0x26] = 0xC0;
            }
            long depth;
            MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x18), F(p, 0x28), &depth);
            MH_CALL(Gte_PrimDepths3_10B)(p);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x34);
        }
        i = n;
    } while (i < 6);
}

// original 0x4C7A80: pushes the matrix and loads Camera_Matrix x the ring's:
// translation RotTrans of (+0x34 >> 9 - 0x4000, +0x38 >> 9 - 0x4000,
// -(+0x3E / 2)), rotation (0, 0, +0xA << 4). Pops nothing - its caller does.
// MagicFx_PushActorMatrix's shape with a turn; the MATRIX one block with the
// translation at +0x14, as the original's stack lays it out.
MS21_EXPORT void __cdecl FrostRing_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const sc = Sprite_Current;
    short rot[4];
    rot[0] = 0;
    rot[1] = 0;
    rot[2] = static_cast<short>(sc[0xA] << 4);
    rot[3] = 0;
    short v[4];
    v[0] = static_cast<short>((Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(sc + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(sc + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    MH_CALL(Gte_RotTrans)(v, m.t);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4C7B30: 32 gouraud triangles in a disc at the owner's screen
// point (+0x2E, +0x30), radius by the phase: 0x40 in 2, (0x20 - +9) * 2 in
// 3, else +9 * 3 + 0x10 (dword 0x903850); the centre (0x20, 0x20, 0xC0),
// the rim (1, 1, 1); each linked at the ring's position.
MS21_EXPORT void __cdecl FrostRing_DrawDisc(void) {
    const unsigned char* const sc = Sprite_Current;
    if (sc[1] == 2) {
        SetLong(Scr(0), 0x40);
    } else if (sc[1] == 3) {
        SetLong(Scr(0), (0x20 - sc[9]) * 2);
    } else {
        SetLong(Scr(0), sc[9] * 3 + 0x10);
    }
    int a = 0;
    do {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        LinkAtTask(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutF(p, 8, S16(Pointer(at::kOwner) + 0x2E));
        PutF(p, 0xC, S16(Pointer(at::kOwner) + 0x30));
        int v = MH_CALL(Math_Sin)(a);
        PutF(p, 0x18, (Mul(v, L(kScratch)) >> 12) + S16(Pointer(at::kOwner) + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        a += 0x80;
        PutF(p, 0x1C, (Mul(v, L(kScratch)) >> 12) + S16(Pointer(at::kOwner) + 0x30));
        v = MH_CALL(Math_Sin)(a);
        PutF(p, 0x28, (Mul(v, L(kScratch)) >> 12) + S16(Pointer(at::kOwner) + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        const std::int32_t y2 = (Mul(v, L(kScratch)) >> 12) + S16(Pointer(at::kOwner) + 0x30);
        p[4] = 0x20;
        p[5] = 0x20;
        p[6] = 0xC0;
        for (unsigned k : {0x14u, 0x15u, 0x16u}) p[k] = 1;
        PutF(p, 0x2C, y2);
        for (unsigned k : {0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAtTask(2, 0x34);
    } while (a < 0x1000);
}

// ===========================================================================
// MAGIC095: the ice effect

// original 0x4C7CD0 (Magic_Rows row 22): the kind-2 task under the actor's
// matrix, its phase +1 through a five-entry stack table - Iceblast_Start,
// _Chill, _Rise, _Sink, _End. Ours aborts past 4.
MS21_EXPORT void __cdecl Iceblast_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Iceblast_Start, bof3::addr::Iceblast_Chill,
                                                 bof3::addr::Iceblast_Rise, bof3::addr::Iceblast_Sink,
                                                 bof3::addr::Iceblast_End};
    MH_CALL(MagicFx_PushActorMatrix)();
    const unsigned phase = Sprite_Current[1];
    if (phase >= 5) bof3::Fatal("Iceblast_Task: phase %u, past the five-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C7D20: the source sprite's (read once) screen point and
// position to the task; the 56 jitter bytes at 0x695958 each Rand & 0xF,
// negated when a first Rand is odd; twelve shard children (kind 1, 0x17)
// owned by it with the source's actor byte and the task's position, +9
// 0x30 - 4 i (the delay), +0xA 0, +0xB i; sound 0x100; +9, +0xA 0; on.
MS21_EXPORT void __cdecl Iceblast_Start(void) {
    const unsigned char* const src = Pointer(at::kSource);
    SetWord(Sprite_Current + 0x2E, Word(src + 0x2E));
    SetWord(Sprite_Current + 0x30, Word(src + 0x30));
    SetLong(Sprite_Current + 0x34, Long(src + 0x34));
    SetLong(Sprite_Current + 0x38, Long(src + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(src + 0x3C));
    for (unsigned i = 0; i < 0x38; ++i) {
        unsigned char v;
        if (MH_CALL(Rand)() & 1)
            v = U8(-(MH_CALL(Rand)() & 0xF));
        else
            v = U8(MH_CALL(Rand)() & 0xF);
        Mem(kJitter + i)[0] = v;
    }
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned k = MH_CALL(BattleTask_Create)(1, 0x17);
        unsigned char* const child = Mem(at::kTasks + k * at::kTaskStride);
        const unsigned char* const sc = Sprite_Current;
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(sc)));
        child[8] = src[8];
        SetLong(child + 0x34, Long(sc + 0x34));
        SetLong(child + 0x38, Long(sc + 0x38));
        SetLong(child + 0x3C, Long(sc + 0x3C));
        child[9] = U8(0x30 - U8(i << 2));
        child[0xA] = 0;
        child[0xB] = U8(i);
    }
    MH_CALL(Sound_PlayById)(0x100);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4C7E30: the disc; at +0xA 0xC the target's tint released and
// set to (6, 4, 8, 0) and its flags 0x200; +0xA up, at 0x10 the phase on.
// The task is read again only after the tint.
MS21_EXPORT void __cdecl Iceblast_Chill(void) {
    MH_CALL(Iceblast_DrawDisc)();
    unsigned char* sc = Sprite_Current;
    if (sc[0xA] == 0xC) {
        unsigned char* const r = TargetRecord();
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 6, 4, 8, 0);
        MH_CALL(Battle_SetTargetFlags)(Mem(at::kTarget)[0], 0x200);
        sc = Sprite_Current;
    }
    Bump(sc[0xA]);
    unsigned char* const s2 = Sprite_Current;
    if (s2[0xA] == 0x10) Bump(s2[1]);
}

// original 0x4C7EC0: the spires, the inner spires, the disc; +9 up, at 0x40
// sounds 0x101 and 0x102, +9 0x30, the phase on.
MS21_EXPORT void __cdecl Iceblast_Rise(void) {
    MH_CALL(Iceblast_DrawSpires)();
    MH_CALL(Iceblast_DrawSpiresInner)();
    MH_CALL(Iceblast_DrawDisc)();
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x40) return;
    MH_CALL(Sound_PlayById)(0x101);
    MH_CALL(Sound_PlayById)(0x102);
    Sprite_Current[9] = 0x30;
    Bump(Sprite_Current[1]);
}

// original 0x4C7F20: the three draws; +9 down, at 0 the phase on.
MS21_EXPORT void __cdecl Iceblast_Sink(void) {
    MH_CALL(Iceblast_DrawSpires)();
    MH_CALL(Iceblast_DrawSpiresInner)();
    MH_CALL(Iceblast_DrawDisc)();
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0) Bump(sc[1]);
}

// original 0x4C7F50: the disc; +0xA down, at 0 the target's tint released,
// it flashed, its flag 0x40, the done flag, free.
MS21_EXPORT void __cdecl Iceblast_End(void) {
    MH_CALL(Iceblast_DrawDisc)();
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(TargetRecord());
    MH_CALL(BattleActor_Flash)(Mem(at::kTarget)[0]);
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// The spires' shared column: vertex A0 at the jitter byte j of row i (radius
// 0x100 - 41 i + j[0], height -72 i - j[0]) and A8 one row up (radius 0xD7 -
// 41 i + j[8], height -72 (i + 1) - j[8]), at angle a; the top row (i 5)
// closes to the axis (A8 x, z 0). The scratch dwords 0x903850 (the radius)
// and 0x903854 (the row's base) as the original leaves them.
struct Spire {
    int base0, base1, h0, h1;
};
static Spire SpireRow(int i) {
    const int b = -41 * i;
    return {b + 0x100, b + 0xD7, -72 * i, -72 * (i + 1)};
}
static void SpireColumn(int i, const Spire& s, int a, unsigned idx) {
    SetLong(Scr(4), s.base0);
    SetLong(Scr(0), static_cast<signed char>(Mem(kJitter + idx)[0]) + s.base0);
    SetWord(Vtx(0), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), L(kScratch)) >> 12));
    SetWord(Vtx(2), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a), L(kScratch)) >> 12));
    SetWord(Vtx(4), static_cast<unsigned>(s.h0 - static_cast<signed char>(Mem(kJitter + idx)[0])));
    SetLong(Scr(4), s.base1);
    SetLong(Scr(0), static_cast<signed char>(Mem(kJitter + idx + 8)[0]) + s.base1);
    SetWord(Vtx(8), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), L(kScratch)) >> 12));
    const int c = MH_CALL(Math_Cos)(a);
    const signed char j1 = static_cast<signed char>(Mem(kJitter + idx + 8)[0]);
    SetWord(Vtx(0xA), static_cast<unsigned>(Mul(c, L(kScratch)) >> 12));
    SetWord(Vtx(0xC), static_cast<unsigned>(s.h1 - j1));
    if (i == 5) {
        SetWord(Vtx(0xA), 0);
        SetWord(Vtx(8), 0);
    }
}

// original 0x4C7FE0: six rows of eight quads (a POLY_G4-shaped primitive,
// 0x5A76F0's code 0x5C) round the actor, each row's column at angle 0x200 k
// jittered by the row's bytes (k & 7); the quad's far edge the column before
// it, swapped (B0 = the old A8, B8 = the old A0). Shade by row: (+9 - 7 i) *
// 6 at most 0x30, nothing drawn at 0 or below; odd columns lit on the near
// edge (A0 / B0), even on the far (A8 / B8), the rest (1, 1, 1).
MS21_EXPORT void __cdecl Iceblast_DrawSpires(void) {
    int i = 0;
    do {
        const Spire s = SpireRow(i);
        SpireColumn(i, s, 0, static_cast<unsigned>(i * 8));
        int k = 1;
        for (int a = 0x200; a < 0x1200; a += 0x200, ++k) {
            const std::uint16_t ax = Word(Vtx(8)), az = Word(Vtx(0xA)), ah = Word(Vtx(0xC));
            SetWord(Vtx(0x14), ah);
            const std::uint16_t cz = Word(Vtx(2));
            SetWord(Vtx(0x10), ax);
            const std::uint16_t ch = Word(Vtx(4));
            SetWord(Vtx(0x1A), cz);
            SetWord(Vtx(0x12), az);
            const std::uint16_t cx = Word(Vtx(0));
            SetWord(Vtx(0x1C), ch);
            SetWord(Vtx(0x18), cx);
            SpireColumn(i, s, a, static_cast<unsigned>((k & 7) + i * 8));
            const unsigned char* const sc = Sprite_Current;
            const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(0))) << 9) +
                                                               static_cast<std::uint32_t>(Long(sc + 0x34)));
            const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(2))) << 9) +
                                                               static_cast<std::uint32_t>(Long(sc + 0x38)));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_AT(PrimFn, kSetPoly5C)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), F(p, 8), F(p, 0x18), F(p, 0x28), F(p, 0x38),
                                       &depth);
            MH_CALL(Gte_PrimDepths4_10C)(p);
            int shade = Sprite_Current[9] - 7 * i;
            if (shade <= 0) continue;
            shade *= 6;
            SetLong(Scr(8), shade);
            if (shade > 0x30) {
                shade = 0x30;
                SetLong(Scr(8), 0x30);
            }
            if (k & 1) {
                p[4] = U8(shade);
                p[5] = Scr(8)[0];
                p[6] = Scr(8)[0];
                p[0x14] = 1;
                p[0x15] = 1;
                p[0x16] = 1;
                p[0x24] = Scr(8)[0];
                p[0x25] = Scr(8)[0];
                p[0x26] = Scr(8)[0];
                p[0x34] = 1;
                p[0x35] = 1;
                p[0x36] = 1;
            } else {
                p[4] = 1;
                p[5] = 1;
                p[6] = 1;
                p[0x14] = Scr(8)[0];
                p[0x15] = Scr(8)[0];
                p[0x16] = Scr(8)[0];
                p[0x24] = 1;
                p[0x25] = 1;
                p[0x26] = 1;
                p[0x34] = Scr(8)[0];
                p[0x35] = Scr(8)[0];
                p[0x36] = Scr(8)[0];
            }
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
        }
        ++i;
    } while (i < 6);
}

// original 0x4C83E0: the same rows as Iceblast_DrawSpires drawn as POLY_G4
// (Gpu_SetPolyG4, Gte_PrimDepths4_10B), the far edge the column before it
// unswapped (B0 = the old A0, B8 = the old A8); shade d = +9 - 8 i: nothing
// at 0 or below, below 5 the blue of every vertex 48 d, else the first and
// third (48 d - 192 at most 0xC0) with the others' 0xC0.
MS21_EXPORT void __cdecl Iceblast_DrawSpiresInner(void) {
    int i = 0;
    do {
        const Spire s = SpireRow(i);
        SpireColumn(i, s, 0, static_cast<unsigned>(i * 8));
        int k = 1;
        for (int a = 0x200; a < 0x1200; a += 0x200, ++k) {
            const std::uint16_t fh = Word(Vtx(0xC)), fz = Word(Vtx(0xA));
            const std::uint16_t nx = Word(Vtx(0));
            SetWord(Vtx(0x1C), fh);
            SetWord(Vtx(0x10), nx);
            SetWord(Vtx(0x12), Word(Vtx(2)));
            SetWord(Vtx(0x14), Word(Vtx(4)));
            const std::uint16_t fx = Word(Vtx(8));
            SetWord(Vtx(0x18), fx);
            SetWord(Vtx(0x1A), fz);
            SpireColumn(i, s, a, static_cast<unsigned>((k & 7) + i * 8));
            const unsigned char* const sc = Sprite_Current;
            const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(0))) << 9) +
                                                               static_cast<std::uint32_t>(Long(sc + 0x34)));
            const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(2))) << 9) +
                                                               static_cast<std::uint32_t>(Long(sc + 0x38)));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), F(p, 8), F(p, 0x18), F(p, 0x28), F(p, 0x38),
                                       &depth);
            MH_CALL(Gte_PrimDepths4_10B)(p);
            const int d = Sprite_Current[9] - (i << 3);
            if (d <= 0) continue;
            if (d < 5) {
                SetLong(Scr(8), d * 48);
                if (d * 48 > 0xC0) SetLong(Scr(8), 0xC0);   // the original's compare; d <= 4 never meets it
                p[4] = 1;
                p[5] = 1;
                p[0x14] = 1;
                p[6] = Scr(8)[0];
                p[0x15] = 1;
                p[0x16] = 1;
                p[0x24] = 1;
                p[0x25] = 1;
                p[0x26] = Scr(8)[0];
                p[0x34] = 1;
                p[0x35] = 1;
                p[0x36] = 1;
            } else {
                const int v = (d * 3 - 0xC) << 4;
                SetLong(Scr(8), v);
                if (v > 0xC0) SetLong(Scr(8), 0xC0);
                p[4] = 1;
                p[5] = 1;
                p[6] = 0xC0;
                p[0x14] = 1;
                p[0x15] = 1;
                p[0x16] = Scr(8)[0];
                p[0x24] = 1;
                p[0x25] = 1;
                p[0x26] = 0xC0;
                p[0x34] = 1;
                p[0x35] = 1;
                p[0x36] = Scr(8)[0];
            }
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
        }
        ++i;
    } while (i < 6);
}

// original 0x4C87E0: a disc of sixteen gouraud triangles, radius 0x1C0 (dword
// 0x903850), round the origin; the centre (+0xA * 8) grey, the rim (1, 1,
// 1); each linked at the task's position.
MS21_EXPORT void __cdecl Iceblast_DrawDisc(void) {
    SetLong(Scr(0), 0x1C0);
    SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), L(kScratch)) >> 12));
    SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), L(kScratch)) >> 12));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const std::uint16_t y = Word(Vtx(0x12)), x = Word(Vtx(0x10));
        SetWord(Vtx(0), 0);
        SetWord(Vtx(2), 0);
        SetWord(Vtx(4), 0);
        SetWord(Vtx(8), x);
        SetWord(Vtx(0xA), y);
        SetWord(Vtx(0xC), 0);
        SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), L(kScratch)) >> 12));
        const int c = MH_CALL(Math_Cos)(a);
        unsigned char* const mode = Gfx_PacketNext;
        SetWord(Vtx(0x12), static_cast<unsigned>(Mul(c, L(kScratch)) >> 12));
        SetWord(Vtx(0x14), 0);
        MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
        LinkAtTask(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::int32_t grey = Sprite_Current[0xA] << 3;
        SetLong(Scr(8), grey);
        p[4] = U8(grey);
        p[5] = Scr(8)[0];
        p[6] = Scr(8)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x18), F(p, 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        LinkAtTask(2, 0x34);
    }
}

// --- the shard child (kind 1, parameter 0x17) -------------------------------------

// original 0x4C8970: under the actor's matrix, its phase through [0x65B64C
// + 4 * +1] - IceShard_Wait, IceShard_Run.
MS21_EXPORT void __cdecl IceShard_Task(void) {
    MH_CALL(MagicFx_PushActorMatrix)();
    CallCell(kShardPhases, Sprite_Current[1]);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C8990: held until the owner's phase is 3 (Iceblast_Sink); then
// +9 (its delay) down, at 0 the phase on.
MS21_EXPORT void __cdecl IceShard_Wait(void) {
    if (Pointer(at::kOwner)[1] != 3) return;
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0) Bump(sc[1]);
}

// original 0x4C89C0: the shards; +9 up, at 0x20 free.
MS21_EXPORT void __cdecl IceShard_Run(void) {
    MH_CALL(IceShard_Draw)();
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] == 0x20) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C89F0: 32 flat triangles (POLY_F3, 0x5A7570) on a ring of row
// h = +0xB >> 1 of the jitter bytes at angle 0x80 k: radius 0x100 - 41 h +
// j[(k / 8) & 7] + 4 (+0xA, or +9 while +0xA is 0), height -72 h - j (0x24
// higher for odd +0xB), each point moved by the jitter byte j[h + k]; the
// triangle's three corners a small circle (radius 0x21 - +9) round that
// point at (+9 + 0x10, +9 - 0x18, +9 - 0xF) & 0x3F << 6, their height
// + +9 * 4 - clamped to 0 when above, which also latches +0xA = +9 once.
// Colour (0x40, 0x40, 0x40 + Rand & 0x7F); linked at the point.
MS21_EXPORT void __cdecl IceShard_Draw(void) {
    const unsigned char* sc = Sprite_Current;
    const int h = sc[0xB] >> 1;
    const std::int32_t base = 0x100 - 41 * h;
    const std::int32_t lift = -72 * h;
    int k = 1;
    for (int a = 0x80; a < 0x1080; a += 0x80, ++k) {
        sc = Sprite_Current;
        SetLong(Scr(4), base);
        const int col = (k / 8) & 7;
        const unsigned char* const jit = Mem(kJitter + static_cast<std::uint32_t>(col + h * 8));
        std::int32_t radius = static_cast<signed char>(jit[0]) + base;
        SetLong(Scr(0), radius);
        if (sc[0xA] != 0)
            SetLong(Scr(0), radius + sc[0xA] * 4);
        else
            SetLong(Scr(0), radius + sc[9] * 4);
        const int s = MH_CALL(Math_Sin)(a);
        std::uint16_t px = static_cast<std::uint16_t>(Mul(s, L(kScratch)) >> 12);
        const int c = MH_CALL(Math_Cos)(a);
        std::uint16_t pz = static_cast<std::uint16_t>(Mul(c, L(kScratch)) >> 12);
        std::uint16_t py = static_cast<std::uint16_t>(lift - static_cast<signed char>(jit[0]));
        const unsigned char* const s2 = Sprite_Current;
        if (s2[0xB] & 1) py = static_cast<std::uint16_t>(py - 0x24);
        const std::uint16_t j = static_cast<std::uint16_t>(static_cast<signed char>(Mem(kJitter + static_cast<std::uint32_t>(h + k))[0]));
        py = static_cast<std::uint16_t>(py + j);
        px = static_cast<std::uint16_t>(px + j);
        pz = static_cast<std::uint16_t>(pz + j);
        const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(static_cast<short>(px)) << 9) +
                                                           static_cast<std::uint32_t>(Long(s2 + 0x34)));
        const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(static_cast<short>(pz)) << 9) +
                                                           static_cast<std::uint32_t>(Long(s2 + 0x38)));
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_AT(PrimFn, kSetPolyF3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetLong(Scr(0), 0x21 - Sprite_Current[9]);
        int v = MH_CALL(Math_Sin)(((Sprite_Current[9] + 0x10) & 0x3F) << 6);
        SetWord(Vtx(0), static_cast<unsigned>((Mul(v, L(kScratch)) >> 12) + px));
        v = MH_CALL(Math_Cos)(((Sprite_Current[9] + 0x10) & 0x3F) << 6);
        SetWord(Vtx(2), static_cast<unsigned>((Mul(v, L(kScratch)) >> 12) + pz));
        v = MH_CALL(Math_Sin)(((Sprite_Current[9] - 0x18) & 0x3F) << 6);
        SetWord(Vtx(8), static_cast<unsigned>((Mul(v, L(kScratch)) >> 12) + px));
        v = MH_CALL(Math_Cos)(((Sprite_Current[9] - 0x18) & 0x3F) << 6);
        SetWord(Vtx(0xA), static_cast<unsigned>((Mul(v, L(kScratch)) >> 12) + pz));
        v = MH_CALL(Math_Sin)(((Sprite_Current[9] - 0xF) & 0x3F) << 6);
        SetWord(Vtx(0x10), static_cast<unsigned>((Mul(v, L(kScratch)) >> 12) + px));
        v = MH_CALL(Math_Cos)(((Sprite_Current[9] - 0xF) & 0x3F) << 6);
        unsigned char* const s3 = Sprite_Current;
        SetWord(Vtx(0x12), static_cast<unsigned>((Mul(v, L(kScratch)) >> 12) + pz));
        const std::uint16_t y = static_cast<std::uint16_t>(py + s3[9] * 4u);
        SetWord(Vtx(0x14), y);
        SetWord(Vtx(0xC), y);
        SetWord(Vtx(4), y);
        if (static_cast<short>(y) > 0) {
            SetWord(Vtx(0x14), 0);
            SetWord(Vtx(0xC), 0);
            SetWord(Vtx(4), 0);
            if (s3[0xA] == 0) s3[0xA] = s3[9];
        }
        p[4] = 0x40;
        p[5] = 0x40;
        p[6] = U8((MH_CALL(Rand)() & 0x7F) + 0x40);
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x14), F(p, 0x20), &depth);
        MH_CALL(Gte_PrimDepths3_0C)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x2C);
    }
}

void MagicS21_Inject() {
    if (bof3::WantsShadow("magic_s21")) magic_s21::SelfTest();
    BOF3_INJECT(Inferno_Task);
    BOF3_INJECT(Inferno_Start);
    BOF3_INJECT(Inferno_End);
    BOF3_INJECT(FxScorch_Dispatch);
    BOF3_INJECT(FxScorch_Run);
    BOF3_INJECT(FxScorch_Delay);
    BOF3_INJECT(FxScorch_Hit);
    BOF3_INJECT(FxScorch_Free);
    BOF3_INJECT(FlamePool_Dispatch);
    BOF3_INJECT(FlameColumn_Task);
    BOF3_INJECT(FlameColumn_Start);
    BOF3_INJECT(FlameColumn_Grow);
    BOF3_INJECT(FlameColumn_Hold);
    BOF3_INJECT(FlameColumn_Fade);
    BOF3_INJECT(FlameColumn_DrawDisc);
    BOF3_INJECT(FlameColumn_DrawBand);
    BOF3_INJECT(FlameSpark_Task);
    BOF3_INJECT(FlameSpark_Start);
    BOF3_INJECT(FlameSpark_Grow);
    BOF3_INJECT(FlameSpark_Stretch);
    BOF3_INJECT(FlameSpark_Fade);
    BOF3_INJECT(FlameSpark_Draw);
    BOF3_INJECT(FlamePool_Alloc);
    BOF3_INJECT(Inferno_TargetCentre);
    BOF3_INJECT(Frost_Task);
    BOF3_INJECT(Frost_Start);
    BOF3_INJECT(Frost_Wait);
    BOF3_INJECT(FrostRing_Task);
    BOF3_INJECT(FrostRing_Mark);
    BOF3_INJECT(FrostRing_Grow);
    BOF3_INJECT(FrostRing_Spin);
    BOF3_INJECT(FrostRing_Fade);
    BOF3_INJECT(FrostRing_DrawShards);
    BOF3_INJECT(FrostRing_PushMatrix);
    BOF3_INJECT(FrostRing_DrawDisc);
    BOF3_INJECT(Iceblast_Task);
    BOF3_INJECT(Iceblast_Start);
    BOF3_INJECT(Iceblast_Chill);
    BOF3_INJECT(Iceblast_Rise);
    BOF3_INJECT(Iceblast_Sink);
    BOF3_INJECT(Iceblast_End);
    BOF3_INJECT(Iceblast_DrawSpires);
    BOF3_INJECT(Iceblast_DrawSpiresInner);
    BOF3_INJECT(Iceblast_DrawDisc);
    BOF3_INJECT(IceShard_Task);
    BOF3_INJECT(IceShard_Wait);
    BOF3_INJECT(IceShard_Run);
    BOF3_INJECT(IceShard_Draw);
}
