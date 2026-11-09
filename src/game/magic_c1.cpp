// Six spell overlays compiled into the exe, round nine group C1
// (docs/magic_c1.md): the unfinished skills TCRF lists, as the PC's code has
// them (docs/cut-content.md section 2). The names are this project's, from
// what the code does; TCRF's names are third-party labels for the PlayStation
// release and are only cited in the doc.
//
//   - MAGIC010 0x49DEF0..0x49DFF4 (row 9, TCRF's White Flag): the source sprite
//     tinted and faded back, the target actor flashed;
//   - MAGIC080 0x4FC330..0x4FC696 (row 27, shared by many ids): a row of
//     glyphs drawn over a point for 0x30 frames - one time in 64 a row of six
//     characters from the exe's strings instead;
//   - MAGIC113 0x4D67F0..0x4D7952 (row 10, TCRF's Pentagram): a disc, two
//     rings, a five-line figure, a band of quads and six sprites, in turn;
//   - MAGIC145 0x4E9A70..0x4E9EE0 (row 149, TCRF's Ink) and MAGIC146
//     0x4E9EF0..0x4EA446 (row 150, Ink Ink): textured puffs over the source
//     sprite (145) or over every living actor of the target's side (146);
//   - MAGIC213 0x4F4A60..0x4F50A6 (row 148): each living actor of a side
//     tinted up and down, seven sprite motes circling it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table (a stack table, or a .data table read in place) aborts where
// the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_c1.h"

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
// sixteen bytes (0x903850..0x90385F) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF), read again after every call as the
// originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

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

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and the other units' called by address, as the
// originals call them: in the game Capcom's code or the jmp Inject put there,
// in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Other groups' functions and the phase handlers a stack table holds are ours
// now and called by name (bof3::addr): Port_DroppedCall (MAGIC124's extent, a
// bare ret) and S37's MagicFx_FreeCurrentRecord among them.

// The sprite animation bank pointer Sprite_SetAnimationAt reads (0x8B3580 the
// area's, 0x8C5D80 the battle's, 0x8E3580 the spell's).
// kFrameSet: now BattleE2_AnimSet (symbols.toml).

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

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC010 (row 9)

// original 0x49DEF0: the kind-2 task. A four-entry stack table by +1:
// WhiteFlag_TintSource, MAGIC117's tint-up (0x4D9FD0), WhiteFlag_Untint, the
// engine's end (0x43F460).
C1_EXPORT void __cdecl WhiteFlag_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::WhiteFlag_TintSource, bof3::addr::Magic117_Brighten,
                                                 bof3::addr::WhiteFlag_Untint, bof3::addr::MagicFx_FlagTargetEnd};
    StackCall(kPhases, 4, Sc()[1], "WhiteFlag_Task");
}

// original 0x49DF30: the source sprite's tints released and a tint (0, 0, 0,
// 1) set on it, its record kept in +0xB; +9 0; +1 on.
C1_EXPORT void __cdecl WhiteFlag_TintSource(void) {
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[0xB] = tint;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x49DF70: the tint record +0xB's colour bytes down by one; +9 down;
// at 0 the source's tints released, the target actor flashed, +1 on. The
// record index is unbounded, as in the original (up to 255 records of 12
// bytes, inside MoveScript_TintRecords' 3,072).
C1_EXPORT void __cdecl WhiteFlag_Untint(void) {
    unsigned char* const s = Sc();
    for (unsigned k = 2; k <= 4; ++k) Dec(MoveScript_TintRecords[s[0xB] * 12u + k]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

// ===========================================================================
// MAGIC080 (row 27)

// original 0x4FC330: the kind-2 task. A two-entry stack table by +1:
// Magic080_Start, Magic080_Run.
C1_EXPORT void __cdecl Magic080_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic080_Start, bof3::addr::Magic080_Run};
    StackCall(kPhases, 2, Sc()[1], "Magic080_Task");
}

// original 0x4FC360: the task at (Field_Kind2X, Field_Kind2Z) and the map's
// elevation there; +0xB Rand & 0x3F (0 picks the text); +9 0x30; +1 on.
C1_EXPORT void __cdecl Magic080_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(Field_Kind2X));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(Field_Kind2Z));
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const long h = MH_CALL(AreaMap_Elevation)(ts->x, ts->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<unsigned>(h));
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0x3F);
    Sc()[9] = 0x30;
    Inc(Sc()[1]);
}

// original 0x4FC3C0: the screen point, its x 0x24 to the left; the glyph row
// (+0xB not 0) or the text (0); +9 down, at 0 the target's flag 0x40, the
// done flag, the task freed.
C1_EXPORT void __cdecl Magic080_Run(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) - 0x24u);
    if (Sc()[0xB] != 0)
        Call0(bof3::addr::Magic080_DrawGlyphs);
    else
        Call0(bof3::addr::Magic080_DrawText);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4FC420: six characters, one from the head of each string the
// six pointers at 0x66A3C8 name (its first byte & 0x7F, its second: the
// character's code at +0x16), each a code-0x6C primitive 12 wide and 12 high
// from the screen point (+0x2E, +0x30) on, grey 0x80, CLUT (0, 0x1E0),
// committed to slot 2.
C1_EXPORT void __cdecl Magic080_DrawText(void) {
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetCode6C)(p);
        const unsigned x = i * 12;
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x1, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x + 12);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y1, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x2, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y2, Word(Sc() + 0x30) + 12u);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x3, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x + 12);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y3, Word(Sc() + 0x30) + 12u);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        const unsigned char* const str = Magic080_TextPointers[i];
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->glyph, ((str[0] & 0x7Fu) << 8) + str[1]);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E0));
        p[0xC] = 0;
        p[0xD] = 0;
        p[0x14] = 12;
        p[0x15] = 0;
        p[0x1C] = 0;
        p[0x1D] = 12;
        p[0x24] = 12;
        p[0x25] = 12;
        MH_CALL(Gfx_CommitPrim)(2, 0x28);
    }
}

// original 0x4FC540: seven glyphs, textured quads 12 wide and 12 high from
// the screen point on, grey 0x80, page (0x3C0, 0) with blend 1, CLUT (0,
// 0x1E0), each cell's u / v the byte pair at 0x65DA28 + 4 i / + 2; slot 2.
C1_EXPORT void __cdecl Magic080_DrawGlyphs(void) {
    for (unsigned i = 0; i < 7; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        const int x0 = static_cast<int>(i * 12), x1 = x0 + 12;
        PutFloat(p + 8, S16(Sc() + 0x2E) + x0);
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        PutFloat(p + 0x18, S16(Sc() + 0x2E) + x1);
        PutFloat(p + 0x1C, S16(Sc() + 0x30));
        PutFloat(p + 0x28, S16(Sc() + 0x2E) + x0);
        PutFloat(p + 0x2C, S16(Sc() + 0x30) + 12);
        PutFloat(p + 0x38, S16(Sc() + 0x2E) + x1);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        PutFloat(p + 0x3C, S16(Sc() + 0x30) + 12);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0));
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E0));
        const unsigned char* const uv = Magic080_GlyphCells + 4 * i;
        p[0x14] = uv[0];
        p[0x15] = uv[2];
        p[0x24] = static_cast<unsigned char>(uv[0] + 12);
        p[0x25] = uv[2];
        p[0x34] = uv[0];
        p[0x35] = static_cast<unsigned char>(uv[2] + 12);
        p[0x44] = static_cast<unsigned char>(uv[0] + 12);
        p[0x45] = static_cast<unsigned char>(uv[2] + 12);
        MH_CALL(Gfx_CommitPrim)(2, 0x48);
    }
}

// ===========================================================================
// MAGIC113 (row 10)

// ===========================================================================
// MAGIC145 (row 149) and MAGIC146 (row 150)

namespace {

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

// kInkPool: now InkInkPuff_Pool (symbols.toml). InkInkPuff_Pool
// kMotePool: now Magic213Mote_Pool (symbols.toml). Magic213Mote_Pool

}  // namespace

// ===========================================================================
// MAGIC213 (row 148)

// original 0x4F4A60: the kind-2 task. A two-entry stack table by +1 -
// Magic213_Start, MAGIC131's end once +0xB is 0 (0x4E5200) - then the mote
// pool walked (Magic213Mote_Dispatch).
C1_EXPORT void __cdecl Magic213_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic213_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    StackCall(kPhases, 2, Sc()[1], "Magic213_Task");
    WalkPool(::bof3::Img(Magic213Mote_Pool_addr), bof3::addr::Magic213Mote_Dispatch);
}

// original 0x4F4AE0: the mote pool cleared; +0xB 0, +1 on; a child (kind 1,
// parameter 0x6B) on each living actor of the target's side (+0xB not
// written); row 26's first sixteen CLUT entries from their source, as they
// are; Gfx_ClutStripDirty; sound 0x100.
C1_EXPORT void __cdecl Magic213_Start(void) {
    ClearPool(::bof3::Img(Magic213Mote_Pool_addr));
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    SpawnOnActors(0x6B, false);
    ClutCopy(0x1A00, 0x1A10, 0);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F4C40: the actor child's kind-1 task, a jmp through
// Magic213Actor_Kinds (one entry) by +1.
C1_EXPORT void __cdecl Magic213Actor_Task(void) {
    CellCall(Addr(Magic213Actor_Kinds), 1, Sc()[1], "Magic213Actor_Task");
}

// original 0x4F4C60: a jmp through Magic213Actor_Phases (four entries:
// Start, ActorFx_TintUp, Magic213Actor_Untint, MagicFx_EndWithChildren) by
// +2.
C1_EXPORT void __cdecl Magic213Actor_Run(void) {
    CellCall(Addr(Magic213Actor_Phases), 4, Sc()[2], "Magic213Actor_Run");
}

// original 0x4F4C80: +9 down; at 0: the actor's record (+4, by the target
// byte's side bit, unchecked); the task at its position; +0xB 0, +9 0x10,
// +0xA 0, +2 on; seven motes (+9 1, 5, .. 25) from the pool; the actor's
// tints released and a tint (0, 0, 0, 1) set on it, its record in +0xA.
C1_EXPORT void __cdecl Magic213Actor_Start(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (s[9] != 0) return;
    unsigned char* const rec = ActorRecord((TargetByte() & 0x40) != 0, s[4]);
    ts->x = rv::As<records::SpriteHead>(rec)->x;
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(rec)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(rec)->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
    SpawnRecords(bof3::addr::Magic213Mote_Alloc, ::bof3::Img(Magic213Mote_Pool_addr), 1, 4, 0x1D);
    MH_CALL(Sprite_ReleaseTint)(rec);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(rec, 0, 0, 0, 1);
    Sc()[0xA] = tint;
}

// original 0x4F4DA0 (MAGIC213's; also entry 2 of MAGIC073's and 074's child
// tables): the tint record +0xA's colour bytes up by one; +9 down, at 0 +2
// on.
C1_EXPORT void __cdecl ActorFx_TintUp(void) {
    unsigned char* const s = Sc();
    for (unsigned k = 2; k <= 4; ++k) Inc(MoveScript_TintRecords[s[0xA] * 12u + k]);
    Dec(s[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4F4E10: the tint record +0xA's colour bytes down by one; when
// its first is 0, the record released (Tint_Release), the actor's flag 0x40,
// +2 on.
C1_EXPORT void __cdecl Magic213Actor_Untint(void) {
    unsigned char* const s = Sc();
    for (unsigned k = 2; k <= 4; ++k) Dec(MoveScript_TintRecords[s[0xA] * 12u + k]);
    const unsigned char tint = s[0xA];
    if (MoveScript_TintRecords[tint * 12u + 2] != 0) return;
    MH_CALL(Tint_Release)(tint);
    MH_CALL(Battle_SetTargetFlag40)(Sc()[4]);
    Inc(Sc()[2]);
}

// original 0x4F4EA0: a mote record's dispatch, a jmp through
// Magic213Mote_Kinds (one entry) by +1.
C1_EXPORT void __cdecl Magic213Mote_Dispatch(void) {
    CellCall(Addr(Magic213Mote_Kinds), 1, Sc()[1], "Magic213Mote_Dispatch");
}

// original 0x4F4EC0: with the frame-offset table the effects' (0x8E3580), a
// call through Magic213Mote_Phases (two entries: Start, End) by +2; while +0
// and +2 are set, Sprite_UpdateScreen; the table back (0x8B3580).
C1_EXPORT void __cdecl Magic213Mote_Run(void) {
    const unsigned phase = Sc()[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    CellCall(Addr(Magic213Mote_Phases), 2, phase, "Magic213Mote_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4F4F00: +9 down; at 0 the mote on a circle of radius 12 round
// its owner at the angle (+0xB & 7) x 0x200, 0x180 above it (+0x3C's high
// word); its sprite fields set up (+0x25 0x1D, +0x26 0, +0x27 0xA0, +0x28 0,
// colour 0 with +0x5C 0, +0x2A 0, +0x29 and +0x24 4, +0x2C 0, +0x2B 1);
// animation +0xB % 3; +2 on.
C1_EXPORT void __cdecl Magic213Mote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetSW(0, static_cast<unsigned>(Sc()[0xB] & 7) << 9);
    int v = MH_CALL(Math_Sin)(static_cast<short>(SW(0)));
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x) +
                                                   static_cast<std::uint32_t>(v) * 12u));
    v = MH_CALL(Math_Cos)(static_cast<short>(SW(0)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z) +
                                                   static_cast<std::uint32_t>(v) * 12u));
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + 0x1800000u));
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Sc()[0x5C] = 0;
    Sc()[0x2A] = 0;
    Sc()[0x29] = 4;
    Sc()[0x24] = 4;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[0xB] % 3));
    Inc(Sc()[2]);
}

// original 0x4F5030: the mote's script ticked; when it reports an end, the
// owner's +0xB down and the record freed (0x4F6290).
C1_EXPORT void __cdecl Magic213Mote_End(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F5050: the mote pool's alloc (0x6B2958, 64 records).
C1_EXPORT unsigned char __cdecl Magic213Mote_Alloc(void) { return PoolAlloc(::bof3::Img(Magic213Mote_Pool_addr)); }

void MagicC1_Inject() {
    if (bof3::WantsShadow("magic_c1")) magic_c1::SelfTest();
    BOF3_INJECT(WhiteFlag_Task);
    BOF3_INJECT(WhiteFlag_TintSource);
    BOF3_INJECT(WhiteFlag_Untint);
    BOF3_INJECT(Magic080_Task);
    BOF3_INJECT(Magic080_Start);
    BOF3_INJECT(Magic080_Run);
    BOF3_INJECT(Magic080_DrawText);
    BOF3_INJECT(Magic080_DrawGlyphs);
    BOF3_INJECT(Magic213_Task);
    BOF3_INJECT(Magic213_Start);
    BOF3_INJECT(Magic213Actor_Task);
    BOF3_INJECT(Magic213Actor_Run);
    BOF3_INJECT(Magic213Actor_Start);
    BOF3_INJECT(ActorFx_TintUp);
    BOF3_INJECT(Magic213Actor_Untint);
    BOF3_INJECT(Magic213Mote_Dispatch);
    BOF3_INJECT(Magic213Mote_Run);
    BOF3_INJECT(Magic213Mote_Start);
    BOF3_INJECT(Magic213Mote_End);
    BOF3_INJECT(Magic213Mote_Alloc);
}
