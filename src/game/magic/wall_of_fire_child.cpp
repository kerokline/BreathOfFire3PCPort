// Moved here by tools/move_funcs.py from magic_c1.cpp, magic_s32.cpp (docs/source-layout-round.md).
#include "game/magic/wall_of_fire_child.h"

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

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t Addr(const void* p) { return Low32(p); }

// The actor record by battle index as the originals compute it - an enemy
// (i - 3) * 0x128 from 0x93B960 when the target byte has 0x40, else a party
// member i * 0x14C from 0x802D40 - unchecked, 32-bit arithmetic.
unsigned char* ActorRecord(bool enemies, unsigned i) {
    return enemies ? Mem(::bof3::Img(Battle_Enemies_addr) + (i - 3u) * at::kEnemyStride) : Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and the other units' called by address, as the
// originals call them: in the game Capcom's code or the jmp Inject put there,
// in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using PtrFn = void (__cdecl*)(unsigned char*);

using ByteFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's and other groups' code this group calls (docs/magic_c1.md section 8).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};  // engine: +0xC / +0x10 of a task turned by its +8

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only - what
// follows is the next table or data.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// Gfx_ClutStrip entries first..last - 1 from Gfx_ClutStripSource, with or
// without the STP bit: row 26's first CLUT (0x1A00..0x1A0F, MAGIC113 and 213)
// or entries 1..15 of its third (0x1A21..0x1A2F, MAGIC145 and 146, which then
// clear 0x1A20).
void ClutCopy(unsigned first, unsigned last, std::uint16_t stp) {
    for (unsigned k = first; k < last; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | stp);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The puffs' start: at the base record's position and direction, offset by
// the (dx, dz, dy) row +0xB of `offsets` (12-byte rows, the index unbounded),
// dx / dz turned by the direction (0x446770); the screen point; +9 and +0xA 0;
// +2 on. The base is read once; what it holds is read after the turn.
void PuffPlace(const unsigned char* base, std::uint32_t offsets) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, Long(Mem(offsets + 12u * Sc()[0xB])));
    rv::Store(rv::Current<records::BattleTask>()->step_z, Long(Mem(offsets + 4 + 12u * Sc()[0xB])));
    MH_AT(PtrFn, kTurnOffset.v)(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x) +
                                                   static_cast<std::uint32_t>(rv::As<records::SpriteHead>(base)->x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_z) +
                                                   static_cast<std::uint32_t>(rv::As<records::SpriteHead>(base)->z)));
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Mem(offsets + 8 + 12u * Sc()[0xB]))) +
                                                   static_cast<std::uint32_t>(rv::As<records::SpriteHead>(base)->height)));
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// A pool of 64 records of 0x84 bytes (MAGIC146's, MAGIC213's), run as tasks:
// every record with bit 0 becomes Sprite_Current, its +0x80 the owner, for
// `run`; both put back after each. The task and owner are read after the
// phase call that precedes the walk.
void WalkPool(std::uint32_t pool, std::uint32_t run) {
    unsigned char* const self = Sprite_Current;
    const std::uint32_t owner = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(BattleE2_Owner_addr))));
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(run);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), static_cast<std::int32_t>(owner));
        Sprite_Current = self;
    }
}

void ClearPool(std::uint32_t pool) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// A child task (kind 1, `parameter`) on every actor of the target's side that
// Battle_ActorIsOut does not answer for - the enemies 3..10 when the target
// byte has 0x40, else the party 0..2: its owner this task, +1 0, +4 the actor,
// +9 a delay of the count so far x 16 + 1 (and +0xB the count, `index`), the
// count +0xB up. Sprite_Current is read after each create.
void SpawnOnActors(unsigned parameter, bool index) {
    const bool enemies = (TargetByte() & 0x40) != 0;
    const unsigned first = enemies ? 3 : 0, count = enemies ? 8 : 3;
    for (unsigned a = first; a < first + count; ++a) {
        if (MH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        const unsigned slot = NewTask(parameter);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Addr(self));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(a);
        if (index) child[0xB] = self[0xB];
        child[9] = static_cast<unsigned char>((self[0xB] << 4) + 1);
        Inc(self[0xB]);
    }
}

// Records from a pool's alloc (`alloc`: an index in al, 0xFF when full), one
// per delay first, first + step, .. below `end`: owner this task, +1 0, +0xB
// their number, +9 the delay; counted in this task's +0xB. A full pool skips
// the record, not the number.
void SpawnRecords(std::uint32_t alloc, std::uint32_t pool, unsigned first, unsigned step, unsigned end) {
    for (unsigned i = 0, delay = first; delay < end; ++i, delay += step) {
        const unsigned idx = MH_WORD(ByteFn, alloc)();
        if (idx == 0xFF) continue;
        unsigned char* const rec = Mem(pool + idx * 0x84u);
        unsigned char* const self = Sc();
        SetLong(rec + 0x80, static_cast<std::int32_t>(Addr(self)));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        rec[9] = static_cast<unsigned char>(delay);
        Inc(self[0xB]);
    }
}

// A pool's alloc: the first of 64 records with bit 0 clear gets it, its index
// in al; 0xFF when all are taken.
unsigned char PoolAlloc(std::uint32_t pool) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if (rec[0] & 1) continue;
        rec[0] |= 1;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// MAGIC144's pool: 48 task-like records of 0x84 bytes (+0 bit 0 in use, +1
// kind, +2 step, +0x80 owner), walked by WallOfFire_Task.
// kPool: now WallOfFire_Pool (symbols.toml).
constexpr unsigned kPoolCount = 0x30;

constexpr std::uint32_t kRecord = 0x84;

unsigned char* PoolRecord(unsigned i) { return Mem(::bof3::Img(WallOfFire_Pool_addr) + i * kRecord); }

std::uint32_t Key(const void* p) { return Low32(p); }

int SS(unsigned k) { return static_cast<short>(SW(k)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

int S16MagicS32(const unsigned char* p) { return static_cast<short>(Word(p)); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, (Word(p) + v) & 0xFFFF); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// `imul` then `sar n`: the 32-bit product wraps, the shift is arithmetic.
int MulSar(int a, int b, unsigned n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloatMagicS32(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

int RandCall() { return MH_CALL(Rand)(); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// The GTE projection as the originals push it: one pointer more than the
// prototype names (the flag, in the caller's frame).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

#define S32_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp3(unsigned char* prim) {
    long p, flag;
    S32_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// A task's position (+0x34, +0x38, +0x3C) from another's, read one dword at a
// time as the originals read it.
void CopyPosition(unsigned char* to, const unsigned char* from) {
    rv::Store(rv::As<records::SpriteHead>(to)->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::As<records::SpriteHead>(to)->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::As<records::SpriteHead>(to)->height, rv::As<records::SpriteHead>(from)->height);
}

}  // namespace

#define S32_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4E9140: the kind-2 task. A two-entry stack table by +1
// (WallOfFire_Start, MagicFx_EndWhenChildrenDone); then every pool record with
// +0 bit 0 run (WallOfFireMote_Run) with Sprite_Current the record and
// 0x93B940 its +0x80, both put back to the values read after the phase.
S32_EXPORT void __cdecl WallOfFire_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::WallOfFire_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    StackCall(kPhases, 2, Sc()[1], "WallOfFire_Task");
    unsigned char* const sc = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const m = PoolRecord(i);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(tm->owner);
        Sprite_Current = m;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        Call0(bof3::addr::WallOfFireMote_Run);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = sc;
    }
}

// original 0x4E91C0: +0xB 0; the pool's +0 / +1 / +2 cleared; one child (kind
// 1, parameter 0x36) owned by this task, +1 0, at the source sprite's position
// (the pointer 0x904B4C read before the create, the record after); +0xB 1;
// CLUT strip row 26: cells 1..15 from the source with bit 15, cell 0 cleared;
// dirty; +1 on. The slot is unchecked (0xFF writes past the slots).
S32_EXPORT void __cdecl WallOfFire_Start(void) {
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const m = PoolRecord(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    const unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x36) & 0xFFu;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(sc));
    child[1] = 0;
    CopyPosition(child, source);
    sc[0xB] = 1;
    for (unsigned k = 0x1A01; k < 0x1A10; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = 0;
    Gfx_ClutStripDirty = 1;
    Inc(Sprite_Current[1]);
}

// original 0x4E9270: the child (kind 1, parameter 0x36): a jmp through
// WallOfFireChild_TaskTable (one entry, WallOfFireChild_Run) by +1.
S32_EXPORT void __cdecl WallOfFireChild_Task(void) { CellCall(::bof3::Img(WallOfFireChild_TaskTable_addr), 1, Sc()[1], "WallOfFireChild_Task"); }

// original 0x4E9290: a call through WallOfFireChild_Steps (four: _Spawn,
// _Tint, _WaitFlame, _End) by +2; then while +0 and +2 are set the actor's
// matrix (MagicFx_PushActorMatrix), the disc (WallOfFire_DrawDisc), the
// matrix popped (a tail jmp).
S32_EXPORT void __cdecl WallOfFireChild_Run(void) {
    CellCall(::bof3::Img(WallOfFireChild_Steps_addr), 4, Sc()[2], "WallOfFireChild_Run");
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::WallOfFire_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E92D0: +9 and +0xB 0; three pool records (WallOfFire_PoolAlloc),
// each one the pool answers owned by this child, +0 |= 0x41, +1 its kind (0
// the flame for the first, 1 a spark after), +2 0, +0xB its number, and
// counted in +0xB; sound 0x100; +2 on.
S32_EXPORT void __cdecl WallOfFireChild_Spawn(void) {
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    for (unsigned n = 0; n < 3; ++n) {
        const unsigned char index = MH_AT(ByteFn, bof3::addr::WallOfFire_PoolAlloc)();
        if (index == 0xFF) continue;
        unsigned char* const sc = Sprite_Current;
        unsigned char* const m = PoolRecord(index);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        tm->owner = static_cast<std::int32_t>(Key(sc));
        m[0] = static_cast<unsigned char>(m[0] | 0x41);
        m[1] = n != 0 ? 1 : 0;
        m[2] = 0;
        m[0xB] = static_cast<unsigned char>(n);
        Inc(sc[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    Inc(Sprite_Current[2]);
}

// original 0x4E9370: +9 up by 2; at 0x10 the source sprite's tints released
// and a tint (-8, -8, -8, 1) set on it, the target flagged 0x40, +2 on.
S32_EXPORT void __cdecl WallOfFireChild_Tint(void) {
    AddB(Sc()[9], 2);
    if (Sc()[9] != 0x10) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(Sprite_SetTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)), 0xF8, 0xF8, 0xF8, 1);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Inc(Sprite_Current[2]);
}

// original 0x4E93C0: +2 on once +0xB has bit 7 (the flame sets it,
// Magic092_FlameGrow).
S32_EXPORT void __cdecl WallOfFireChild_WaitFlame(void) {
    unsigned char* const s = Sc();
    if (s[0xB] & 0x80) Inc(s[2]);
}

// original 0x4E93D0: +9 down to 0; once +0xB is 0x80 (the flag with every mote
// gone): the source sprite's tints released, the target actor flashed, the
// owner's +0xB down, the task freed (a tail jmp).
S32_EXPORT void __cdecl WallOfFireChild_End(void) {
    unsigned char* s = Sc();
    if (s[9] != 0) {
        Dec(s[9]);
        s = Sc();
    }
    if (s[0xB] != 0x80) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(BattleActor_Flash)(TargetByte());
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E9420: under the caller's matrix, a disc of eight
// semi-transparent gouraud triangles, radius 0x180 (word 0x903850), round
// the origin at z 0: each from the centre (shade word 0x90385E: +9 x 8 below
// 0x10, else 0x80, in all three channels) to two rim points (shade 1) at
// angles 0x200 apart, the first rim point the last one's second (vertex
// scratch +8 from +0x10); projected by Gte_RotTransPers3, depths by
// Gte_PrimDepths3_10B, committed at 5. Draw modes 0x55 before, 0x15 after.
S32_EXPORT void __cdecl WallOfFire_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSW(0, 0x180);
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(MulSar(v, SS(0), 12)));
    v = Cos(0);
    SetVW(0x12, static_cast<unsigned>(MulSar(v, SS(0), 12)));
    const unsigned char b = Sc()[9];
    SetSW(0xE, b < 0x10 ? b * 8u : 0x80u);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), z = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, z);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(MulSar(v, SS(0), 12)));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(MulSar(v, SS(0), 12)));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        const unsigned char shade = Mem(::bof3::Img(kS.v) + 0xE)[0];
        p[4] = shade;
        p[5] = shade;
        p[6] = shade;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4E95E0: a pool record: a jmp through WallOfFireMote_Kinds (two:
// the flame, the spark) by +1.
S32_EXPORT void __cdecl WallOfFireMote_Run(void) { CellCall(::bof3::Img(WallOfFireMote_Kinds_addr), 2, Sc()[1], "WallOfFireMote_Run"); }

// original 0x4E9600: the flame. A call through WallOfFireFlame_Steps (four:
// _Start, _Rise, Magic092_FlameGrow, _Fade) by +2; then while +0 and +2 are
// set the screen point (BattleActor_UpdateScreenXY) and MAGIC092's flame
// (Magic092_DrawFlame, a tail jmp).
S32_EXPORT void __cdecl WallOfFireFlame_Run(void) {
    CellCall(::bof3::Img(WallOfFireFlame_Steps_addr), 4, Sc()[2], "WallOfFireFlame_Run");
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(Magic092_DrawFlame)();
}

// original 0x4E9630: +0 bit 6 cleared; the owner's position; +9 and +0xA 1;
// +2 on.
S32_EXPORT void __cdecl WallOfFireFlame_Start(void) {
    Sc()[0] = static_cast<unsigned char>(Sc()[0] & 0xBF);
    CopyPosition(Sc(), Owner());
    Sc()[9] = 1;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4E9690: +9 up by 4; at 0x11 +2 on.
S32_EXPORT void __cdecl WallOfFireFlame_Rise(void) {
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x11) Inc(Sc()[2]);
}

// original 0x4E96B0: +9 down by 2, +0xA up on odd frames; at +9 1 the owner's
// +0xB down and +0..+4 cleared (the record free).
S32_EXPORT void __cdecl WallOfFireFlame_Fade(void) {
    AddB(Sc()[9], 0xFE);
    if (static_cast<unsigned char>(Frame_Counter) & 1) Inc(Sc()[0xA]);
    if (Sc()[9] != 1) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x4E9720: a spark. A call through WallOfFireSpark_Steps (four:
// _Start, MagicFx_CountUp9By2, _Wait, _End) by +2; then while +0 and +2 are
// set the screen point, moved 16 left (+0xB 1) or right and 12 up, and the
// spark (WallOfFireSpark_Draw, a tail jmp).
S32_EXPORT void __cdecl WallOfFireSpark_Run(void) {
    CellCall(::bof3::Img(WallOfFireSpark_Steps_addr), 4, Sc()[2], "WallOfFireSpark_Run");
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sprite_Current;
    AddWord(s + 0x2E, s[0xB] == 1 ? 0xFFF0u : 0x10u);
    AddWord(Sprite_Current + 0x30, 0xFFF4u);
    Call0(bof3::addr::WallOfFireSpark_Draw);
}

// original 0x4E9770: +0 bit 6 cleared; the owner's position; +9 0; +2 on.
S32_EXPORT void __cdecl WallOfFireSpark_Start(void) {
    Sc()[0] = static_cast<unsigned char>(Sc()[0] & 0xBF);
    CopyPosition(Sc(), Owner());
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4E97D0: +2 on once the owner's +0xB has bit 7.
S32_EXPORT void __cdecl WallOfFireSpark_Wait(void) {
    if (Owner()[0xB] & 0x80) Inc(Sc()[2]);
}

// original 0x4E97F0: +9 down; at 0 the owner's +0xB down and +0..+4 cleared.
S32_EXPORT void __cdecl WallOfFireSpark_End(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x4E9850: a burst of eight semi-transparent gouraud triangles in
// screen space round the task's screen point (+0x2E, +0x30), radius 0x40 +
// Rand & 15 (word 0x903850), each linked at the task's position (layer 2)
// after a draw-mode packet (0x35): the centre shade (+9 x 10, word 0x90385E)
// red, a fifth of it green and blue; the rim 1.
S32_EXPORT void __cdecl WallOfFireSpark_Draw(void) {
    SetSW(0, (static_cast<unsigned>(RandCall()) & 0xFu) + 0x40u);
    SetSW(0xE, Sc()[9] * 10u);
    for (int a = 0; a < 0x1000;) {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        {
            const unsigned char* const s = Sc();
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z),
                                        2, 0xC);
        }
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloatMagicS32(p + 8, S16MagicS32(Sc() + 0x2E));
        PutFloatMagicS32(p + 0xC, S16MagicS32(Sc() + 0x30));
        int v = Sin(a);
        PutFloatMagicS32(p + 0x18, Add(MulSar(v, SS(0), 12), S16MagicS32(Sc() + 0x2E)));
        v = Cos(a);
        PutFloatMagicS32(p + 0x1C, Add(MulSar(v, SS(0), 12), S16MagicS32(Sc() + 0x30)));
        a += 0x200;
        v = Sin(a);
        PutFloatMagicS32(p + 0x28, Add(MulSar(v, SS(0), 12), S16MagicS32(Sc() + 0x2E)));
        v = Cos(a);
        PutFloatMagicS32(p + 0x2C, Add(MulSar(v, SS(0), 12), S16MagicS32(Sc() + 0x30)));
        p[4] = Mem(::bof3::Img(kS.v) + 0xE)[0];
        const auto fifth = static_cast<unsigned char>(SS(0xE) / 5);
        p[5] = fifth;
        p[6] = fifth;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                    0x34);
    }
}

// original 0x4E9A10: the first of the 48 pool records whose +0 lacks bit 0
// gets it; its index in al, 0xFF when none.
S32_EXPORT unsigned char __cdecl WallOfFire_PoolAlloc(void) {
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const m = PoolRecord(i);
        if ((m[0] & 1) == 0) {
            m[0] = static_cast<unsigned char>(m[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

// original 0x4E9A70: the kind-2 task. A three-entry stack table by +1:
// Ink_Start, MAGIC009's wait for +0xB 0 (0x49DA50), the engine's end
// (0x43F460).
C1_EXPORT void __cdecl Ink_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Ink_Start, bof3::addr::Chlorine_WaitChildren,
                                                 bof3::addr::MagicFx_FlagTargetEnd};
    StackCall(kPhases, 3, Sc()[1], "Ink_Task");
}

// original 0x4E9AA0: +0xB 0, +1 on; three puffs (kind 1, parameter 0x6C: +0xB
// 0..2 their offset, +9 1, 5, 9 their delay), counted in +0xB; row 26's
// entries 1..15 from their source with the STP bit and entry 0 cleared,
// Gfx_ClutStripDirty; sound 0x100; the target's flags 0x10.
C1_EXPORT void __cdecl Ink_Start(void) {
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0, delay = 1; delay < 0xD; ++i, delay += 4) {
        const unsigned slot = NewTask(0x6C);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Addr(self));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(delay);
        Inc(self[0xB]);
    }
    ClutCopy(0x1A21, 0x1A30, 0x8000);
    Gfx_ClutStrip[0x1A20] = 0;
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
}

// original 0x4E9B70: the puff's kind-1 task, a jmp through InkPuff_Kinds
// (one entry) by +1.
C1_EXPORT void __cdecl InkPuff_Task(void) { CellCall(Addr(InkPuff_Kinds), 1, Sc()[1], "InkPuff_Task"); }

// original 0x4E9B90: a call through InkPuff_Phases (three entries: Start,
// then MAGIC009's 0x49DB90 and MAGIC040's 0x4A5D50) by +2; while +0 and +2
// are set, the puff.
C1_EXPORT void __cdecl InkPuff_Run(void) {
    CellCall(Addr(InkPuff_Phases), 3, Sc()[2], "InkPuff_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::Ink_DrawPuff);
}

// original 0x4E9BC0: +9 down; at 0 the puff at the source sprite (its
// direction, its position offset by InkPuff_Offsets[+0xB]).
C1_EXPORT void __cdecl InkPuff_Start(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    const unsigned char* const src = Source();
    s[8] = src[8];
    PuffPlace(src, Addr(InkPuff_Offsets));
}

// original 0x4E9C90 (MAGIC145's and 146's): the puff - a textured quad, page
// (0x340, 0x100) with blend 2, CLUT (0x20, 0x1FA), its corners at the angles
// 0x200, 0x600, 0xE00, 0xA00 of a circle of radius +9 x 2 round the screen
// point, its grey +0xA x 8; a draw mode (0x55) before, sorted at the task's
// position (dy 2).
C1_EXPORT void __cdecl Ink_DrawPuff(void) {
    DrawMode(0x55);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(rv::Current<records::BattleTask>()->x), static_cast<unsigned long>(rv::Current<records::BattleTask>()->z), 2,
                                0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetSW(0, Sc()[9] * 2u);
    SetSW(4, Sc()[0xA] * 8u);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned k = 0; k < 4; ++k) {
        int v = MH_CALL(Math_Cos)(kAngles[k]);
        PutFloat(p + 8 + 0x10 * k, Mul12(v, static_cast<short>(SW(0))) + S16(Sc() + 0x2E));
        v = MH_CALL(Math_Sin)(kAngles[k]);
        PutFloat(p + 0xC + 0x10 * k, Mul12(v, static_cast<short>(SW(0))) + S16(Sc() + 0x30));
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0x20, 0x1FA));
    p[0x14] = 0x80;
    p[0x34] = 0x80;
    p[0x15] = 0x20;
    p[0x24] = 0xA0;
    p[0x25] = 0x20;
    p[0x35] = 0x40;
    p[0x44] = 0xA0;
    p[0x45] = 0x40;
    p[4] = SB(4);
    p[5] = SB(4);
    p[6] = SB(4);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(rv::Current<records::BattleTask>()->x), static_cast<unsigned long>(rv::Current<records::BattleTask>()->z), 2,
                                0x48);
}

// original 0x4E9EF0: the kind-2 task. A three-entry stack table by +1 -
// InkInk_Start, MAGIC009's wait (0x49DA50), the engine's end (0x43F460) -
// then the puff pool walked (InkInkPuff_Dispatch).
C1_EXPORT void __cdecl InkInk_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::InkInk_Start, bof3::addr::Chlorine_WaitChildren,
                                                 bof3::addr::MagicFx_FlagTargetEnd};
    StackCall(kPhases, 3, Sc()[1], "InkInk_Task");
    WalkPool(::bof3::Img(InkInkPuff_Pool_addr), bof3::addr::InkInkPuff_Dispatch);
}

// original 0x4E9F70: the puff pool cleared; +0xB 0, +9 8, +1 on; a child
// (kind 1, parameter 0x6D) on each living actor of the target's side; row 26
// as Ink_Start's (entries 1..15 with STP, entry 0 cleared), Gfx_ClutStripDirty.
C1_EXPORT void __cdecl InkInk_Start(void) {
    ClearPool(::bof3::Img(InkInkPuff_Pool_addr));
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Inc(Sc()[1]);
    SpawnOnActors(0x6D, true);
    ClutCopy(0x1A21, 0x1A30, 0x8000);
    Gfx_ClutStrip[0x1A20] = 0;
    Gfx_ClutStripDirty = 1;
}

// original 0x4EA0F0: the actor child's kind-1 task, a jmp through
// InkInkActor_Kinds (one entry) by +1.
C1_EXPORT void __cdecl InkInkActor_Task(void) {
    CellCall(Addr(InkInkActor_Kinds), 1, Sc()[1], "InkInkActor_Task");
}

// original 0x4EA110: a jmp through InkInkActor_Phases (two entries: Start,
// MAGIC063's 0x4B3320) by +2.
C1_EXPORT void __cdecl InkInkActor_Run(void) {
    CellCall(Addr(InkInkActor_Phases), 2, Sc()[2], "InkInkActor_Run");
}

// original 0x4EA130: +9 down; at 0: the actor's record (+4, by the target
// byte's side bit, unchecked); sound 0x100; the task at the actor's direction
// and position; +0xB 0, +2 on; three puff records (+9 1, 3, 5) from the pool;
// the actor's flags 0x10.
C1_EXPORT void __cdecl InkInkActor_Start(void) {
    Dec(Sc()[9]);
    const unsigned char* const s = Sc();
    if (s[9] != 0) return;
    const unsigned char* const rec = ActorRecord((TargetByte() & 0x40) != 0, s[4]);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[8] = rec[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(rec)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(rec)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(rec)->height);
    Sc()[0xB] = 0;
    Inc(Sc()[2]);
    SpawnRecords(bof3::addr::InkInkPuff_Alloc, ::bof3::Img(InkInkPuff_Pool_addr), 1, 2, 7);
    MH_CALL(Battle_SetTargetFlags)(Sc()[4], 0x10);
}

// original 0x4EA260: a puff record's dispatch, a jmp through InkInkPuff_Kinds
// (one entry) by +1.
C1_EXPORT void __cdecl InkInkPuff_Dispatch(void) {
    CellCall(Addr(InkInkPuff_Kinds), 1, Sc()[1], "InkInkPuff_Dispatch");
}

// original 0x4EA280: a call through InkInkPuff_Phases (three entries: Start,
// MAGIC009's 0x49DB90, Fade) by +2; while +0 and +2 are set, the puff.
C1_EXPORT void __cdecl InkInkPuff_Run(void) {
    CellCall(Addr(InkInkPuff_Phases), 3, Sc()[2], "InkInkPuff_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::Ink_DrawPuff);
}

// original 0x4EA2B0: +9 down; at 0 the puff at its owner (the actor child:
// its direction, position offset by InkInkPuff_Offsets[+0xB]).
C1_EXPORT void __cdecl InkInkPuff_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    rv::Store(rv::Current<records::BattleTask>()->step_x, Long(Mem(Addr(InkInkPuff_Offsets) + 12u * Sc()[0xB])));
    rv::Store(rv::Current<records::BattleTask>()->step_z, Long(Mem(Addr(InkInkPuff_Offsets) + 4 + 12u * Sc()[0xB])));
    MH_AT(PtrFn, kTurnOffset.v)(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->x) +
                                                   static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->z) +
                                                   static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_z)));
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->height) +
                                                   static_cast<std::uint32_t>(Long(Mem(Addr(InkInkPuff_Offsets) + 8 + 12u * Sc()[0xB])))));
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4EA3B0: +9 up, +0xA down; at 0 the owner's +0xB down and the
// record freed (MAGIC219's 0x4F6290: +0..+4 cleared).
C1_EXPORT void __cdecl InkInkPuff_Fade(void) {
    Inc(Sc()[9]);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4EA3F0: the puff pool's alloc (0x6A6F40, 64 records).
C1_EXPORT unsigned char __cdecl InkInkPuff_Alloc(void) { return PoolAlloc(::bof3::Img(InkInkPuff_Pool_addr)); }

void WallOfFireChild_Inject() {
    BOF3_INJECT(WallOfFire_Task);
    BOF3_INJECT(WallOfFire_Start);
    BOF3_INJECT(WallOfFireChild_Task);
    BOF3_INJECT(WallOfFireChild_Run);
    BOF3_INJECT(WallOfFireChild_Spawn);
    BOF3_INJECT(WallOfFireChild_Tint);
    BOF3_INJECT(WallOfFireChild_WaitFlame);
    BOF3_INJECT(WallOfFireChild_End);
    BOF3_INJECT(WallOfFire_DrawDisc);
    BOF3_INJECT(WallOfFireMote_Run);
    BOF3_INJECT(WallOfFireFlame_Run);
    BOF3_INJECT(WallOfFireFlame_Start);
    BOF3_INJECT(WallOfFireFlame_Rise);
    BOF3_INJECT(WallOfFireFlame_Fade);
    BOF3_INJECT(WallOfFireSpark_Run);
    BOF3_INJECT(WallOfFireSpark_Start);
    BOF3_INJECT(WallOfFireSpark_Wait);
    BOF3_INJECT(WallOfFireSpark_End);
    BOF3_INJECT(WallOfFireSpark_Draw);
    BOF3_INJECT(WallOfFire_PoolAlloc);
    BOF3_INJECT(Ink_Task);
    BOF3_INJECT(Ink_Start);
    BOF3_INJECT(InkPuff_Task);
    BOF3_INJECT(InkPuff_Run);
    BOF3_INJECT(InkPuff_Start);
    BOF3_INJECT(Ink_DrawPuff);
    BOF3_INJECT(InkInk_Task);
    BOF3_INJECT(InkInk_Start);
    BOF3_INJECT(InkInkActor_Task);
    BOF3_INJECT(InkInkActor_Run);
    BOF3_INJECT(InkInkActor_Start);
    BOF3_INJECT(InkInkPuff_Dispatch);
    BOF3_INJECT(InkInkPuff_Run);
    BOF3_INJECT(InkInkPuff_Start);
    BOF3_INJECT(InkInkPuff_Fade);
    BOF3_INJECT(InkInkPuff_Alloc);
}
