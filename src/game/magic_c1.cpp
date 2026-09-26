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

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF), read again after every call as the
// originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }
std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// The actor record by battle index as the originals compute it - an enemy
// (i - 3) * 0x128 from 0x93B960 when the target byte has 0x40, else a party
// member i * 0x14C from 0x802D40 - unchecked, 32-bit arithmetic.
unsigned char* ActorRecord(bool enemies, unsigned i) {
    return enemies ? Mem(at::kEnemies + (i - 3u) * at::kEnemyStride) : Mem(at::kParty + i * at::kPartyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and the other units' called by address, as the
// originals call them: in the game Capcom's code or the jmp Inject put there,
// in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using PtrFn = void (__cdecl*)(unsigned char*);
using ByteFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Capcom's and other groups' code this group calls (docs/magic_c1.md section 8).
constexpr std::uint32_t kTurnOffset = 0x446770;    // engine: +0xC / +0x10 of a task turned by its +8
constexpr std::uint32_t kFreeRecord = 0x4F6290;    // MAGIC219 (group S37): +0..+4 of Sprite_Current cleared
constexpr std::uint32_t kDroppedCall = 0x4DF820;   // Port_DroppedCall, a bare ret (MAGIC124's extent)
// Phase handlers of other units a stack table holds.
constexpr std::uint32_t kTintUp16 = 0x4D9FD0;      // MAGIC117 (group S26): the tint +1 a frame to +9 = 0x10
constexpr std::uint32_t kEndFlag40 = 0x43F460;     // engine row 123's end (group E): flag 0x40, done, free
constexpr std::uint32_t kWaitChildren = 0x49DA50;  // MAGIC009 (group S03): +1 on when +0xB is 0
constexpr std::uint32_t kEndNoChildren = 0x4E5200; // MAGIC131 (group S30): done and free when +0xB is 0
constexpr std::uint32_t kPentagramEnd = 0x4D7BD0;  // MAGIC114 (group S26)

// The sprite frame-offset table pointer the sprite draws read (0x8B3580 the
// battle's, 0x8E3580 the effects').
constexpr std::uint32_t kFrameSet = 0x9039D8;

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

// The GTE projections with the arguments the originals push (the depth and
// flag outputs in the caller's frame).
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define C1_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned v, unsigned char* sxy) {
    long p, flag;
    C1_AS(Rtp1Fn, Gte_RotTransPers)(VP(v), sxy, &p, &flag);
}
void Rtp3(unsigned char* prim) {
    long p, flag;
    C1_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}
void Rtp4(unsigned char* prim) {
    long p, flag;
    C1_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC010 (row 9)

// original 0x49DEF0: the kind-2 task. A four-entry stack table by +1:
// WhiteFlag_TintSource, MAGIC117's tint-up (0x4D9FD0), WhiteFlag_Untint, the
// engine's end (0x43F460).
C1_EXPORT void __cdecl WhiteFlag_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::WhiteFlag_TintSource, kTintUp16, bof3::addr::WhiteFlag_Untint,
                                                 kEndFlag40};
    StackCall(kPhases, 4, Sc()[1], "WhiteFlag_Task");
}

// original 0x49DF30: the source sprite's tints released and a tint (0, 0, 0,
// 1) set on it, its record kept in +0xB; +9 0; +1 on.
C1_EXPORT void __cdecl WhiteFlag_TintSource(void) {
    unsigned char* const s0 = Source();
    MH_CALL(Sprite_ReleaseTint)(s0);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(s0, 0, 0, 0, 1);
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
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(Field_Kind2X));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(Field_Kind2Z));
    const unsigned char* const s = Sc();
    const long h = MH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetWord(Sc() + 0x3E, static_cast<unsigned>(h));
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
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) - 0x24u);
    if (Sc()[0xB] != 0)
        Call0(bof3::addr::Magic080_DrawGlyphs);
    else
        Call0(bof3::addr::Magic080_DrawText);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(at::kFlags)[0] |= 4;
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
        SetWord(p + 8, Word(Sc() + 0x2E) + x);
        SetWord(p + 0xA, Word(Sc() + 0x30));
        SetWord(p + 0x10, Word(Sc() + 0x2E) + x + 12);
        SetWord(p + 0x12, Word(Sc() + 0x30));
        SetWord(p + 0x18, Word(Sc() + 0x2E) + x);
        SetWord(p + 0x1A, Word(Sc() + 0x30) + 12u);
        SetWord(p + 0x20, Word(Sc() + 0x2E) + x + 12);
        SetWord(p + 0x22, Word(Sc() + 0x30) + 12u);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        const unsigned char* const str = Magic080_TextPointers[i];
        SetWord(p + 0x16, ((str[0] & 0x7Fu) << 8) + str[1]);
        SetWord(p + 0xE, MH_CALL(Gpu_GetClut)(0, 0x1E0));
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
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1E0));
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

// original 0x4D67F0: the kind-2 task. An eight-entry stack table by +1 -
// Start, Rings, Star, Band, Sprites, WaitSprites, WaitChildren and MAGIC114's
// 0x4D7BD0 - then, while +0 is set, the disc under the actor's matrix.
C1_EXPORT void __cdecl Pentagram_Task(void) {
    static constexpr std::uint32_t kPhases[8] = {
        bof3::addr::Pentagram_Start,   bof3::addr::Pentagram_Rings,       bof3::addr::Pentagram_Star,
        bof3::addr::Pentagram_Band,    bof3::addr::Pentagram_Sprites,     bof3::addr::Pentagram_WaitSprites,
        bof3::addr::Pentagram_WaitChildren, kPentagramEnd};
    StackCall(kPhases, 8, Sc()[1], "Pentagram_Task");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::Pentagram_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D6860: when the target and the actor are on the same side (both
// below 3 or both 3 and up) the target's flag 0x40, the done flag and the task
// freed - nothing drawn; else the task at the source sprite's position, row
// 26's first sixteen CLUT entries from their source with the STP bit,
// Gfx_ClutStripDirty, +0xB and +9 0, +1 on.
C1_EXPORT void __cdecl Pentagram_Start(void) {
    const unsigned char target = TargetByte();
    const unsigned char actor = Mem(at::kActor)[0];
    if ((target < 3) == (actor < 3)) {
        MH_CALL(Battle_SetTargetFlag40)(target);
        Mem(at::kFlags)[0] |= 4;
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    const unsigned char* const src = Source();
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
    ClutCopy(0x1A00, 0x1A10, 0x8000);
    Gfx_ClutStripDirty = 1;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

namespace {

// A child of MAGIC113's task: kind 1, parameter 3 (PentagramChild_Task), its
// owner this task, its phase +1 `kind` (0 a ring, 1 the figure, 2 the band, 3
// a sprite), its direction the owner's +8 - 2 & 3, +9 and +0xA 0, at the
// task's position and height. Sprite_Current and the owner are read after the
// create.
unsigned char* NewChild(unsigned kind) {
    const unsigned slot = NewTask(3);
    unsigned char* const self = Sc();
    const unsigned char* const owner = Owner();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Addr(self)));
    child[1] = static_cast<unsigned char>(kind);
    child[8] = static_cast<unsigned char>((owner[8] - 2) & 3);
    child[9] = 0;
    child[0xA] = 0;
    SetLong(child + 0x34, Long(self + 0x34));
    SetLong(child + 0x38, Long(self + 0x38));
    SetWord(child + 0x3E, Word(self + 0x3E));
    return child;
}

}  // namespace

// original 0x4D6900: +9 up; at 0x10 two rings (children of kind 0, +0xB 0 and
// 1: their radius), sound 0x101, +1 on.
C1_EXPORT void __cdecl Pentagram_Rings(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x10) return;
    for (unsigned b = 0; b < 2; ++b) NewChild(0)[0xB] = static_cast<unsigned char>(b);
    MH_CALL(Sound_PlayEffect)(0x101);
    Inc(Sc()[1]);
}

// original 0x4D6A30: once the rings set +0xB to 3, the figure (a child of
// kind 1) and +1 on.
C1_EXPORT void __cdecl Pentagram_Star(void) {
    if (Sc()[0xB] != 3) return;
    NewChild(1);
    Inc(Sc()[1]);
}

// original 0x4D6AC0: once the figure sets +0xB to 4, the band (a child of
// kind 2), +1 on, sound 0x102, then the bytes 0x803154 0x7F and 0x92BF14 0.
C1_EXPORT void __cdecl Pentagram_Band(void) {
    if (Sc()[0xB] != 4) return;
    NewChild(2);
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x102);
    Mem(0x803154)[0] = 0x7F;
    Mem(0x92BF14)[0] = 0;
}

// original 0x4D6B70: two dropped calls (Port_DroppedCall, a bare ret, with
// 0x14, 0x10 and with 0x14) every frame; once the band sets +0xB to 5, six
// sprites (children of kind 3: +4 i & 1, +9 (i >> 1) * 8 + 1, +0xB i >> 1),
// +0xB 0, +1 on.
C1_EXPORT void __cdecl Pentagram_Sprites(void) {
    MH_AT(void (__cdecl*)(unsigned, unsigned), kDroppedCall)(0x14, 0x10);
    MH_AT(void (__cdecl*)(unsigned), kDroppedCall)(0x14);
    if (Sc()[0xB] != 5) return;
    unsigned char* self = nullptr;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = NewTask(3);
        self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(self)));
        child[1] = 3;
        child[4] = static_cast<unsigned char>(i & 1);
        child[8] = static_cast<unsigned char>((Owner()[8] - 2) & 3);
        child[9] = static_cast<unsigned char>(((i >> 1) << 3) + 1);
        child[0xA] = 0;
        child[0xB] = static_cast<unsigned char>(i >> 1);
        SetLong(child + 0x34, Long(self + 0x34));
        SetLong(child + 0x38, Long(self + 0x38));
        SetWord(child + 0x3E, Word(self + 0x3E));
    }
    // the task as Sprite_Current was read after the last create
    self[0xB] = 0;
    Inc(Sc()[1]);
}

// original 0x4D6C60: once the six sprites have counted +0xB up to 6, +0xB
// 0x84 (the children's release) and +1 on.
C1_EXPORT void __cdecl Pentagram_WaitSprites(void) {
    unsigned char* const s = Sc();
    if (s[0xB] != 6) return;
    s[0xB] = 0x84;
    Inc(Sc()[1]);
}

// original 0x4D6C80: once the four children have counted +0xB down to 0x80,
// +1 on.
C1_EXPORT void __cdecl Pentagram_WaitChildren(void) {
    unsigned char* const s = Sc();
    if (s[0xB] == 0x80) Inc(s[1]);
}

// original 0x4D6C90: the disc - 32 gouraud triangles from the centre to a
// circle of radius 0x1C0, semi-transparent, the centre's shade +9 x 12 and
// the rim 1; a draw mode (tpage 0x55) before, one (0x15) after, all to slot 5.
C1_EXPORT void __cdecl Pentagram_DrawDisc(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSD(0, 0x1C0);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SD(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SD(0))));
    for (int angle = 0x80; angle < 0x1080; angle += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), z = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, z);
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SD(0))));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SD(0))));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        const std::uint32_t shade = Sc()[9] * 12u;
        SetSD(4, shade);
        p[4] = static_cast<unsigned char>(shade);
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D6E30: the child's kind-1 task, a jmp through
// PentagramChild_Kinds (four entries) by +1: ring, figure, band, sprite.
C1_EXPORT void __cdecl PentagramChild_Task(void) {
    CellCall(Addr(PentagramChild_Kinds), 4, Sc()[1], "PentagramChild_Task");
}

// original 0x4D6E50: a call through PentagramRing_Phases (three entries) by
// +2; while +0 is set, the ring under the actor's matrix, its radius
// PentagramRing_Radii[+0xB] (unbounded) in the scratch dword.
C1_EXPORT void __cdecl PentagramRing_Run(void) {
    CellCall(Addr(PentagramRing_Phases), 3, Sc()[2], "PentagramRing_Run");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    SetSD(0, static_cast<std::uint32_t>(Long(Mem(Addr(PentagramRing_Radii) + 4u * Sc()[0xB]))));
    Call0(bof3::addr::PentagramRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D6EA0: +9 up; at 0x21 the owner's +0xB 3 and +2 on.
C1_EXPORT void __cdecl PentagramRing_Grow(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x21) return;
    Owner()[0xB] = 3;
    Inc(Sc()[2]);
}

// original 0x4D70E0 (the ring's and the figure's): once the owner's +0xB is
// 0x84, +2 on.
C1_EXPORT void __cdecl PentagramFx_WaitRelease(void) {
    if (Owner()[0xB] == 0x84) Inc(Sc()[2]);
}

// original 0x4D7100 (the ring's and the figure's): +0xA up; at 0xC the
// owner's +0xB down and the task freed.
C1_EXPORT void __cdecl PentagramFx_Fade(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0xC) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// The ring's and figure's line colour: (0xF0, 0xAA, 0xC8), dimmed by +0xA
// (x 20, x 14, x 16) while +2 is 2 (the fade), each byte read again.
void LineColour(unsigned char* p) {
    p[4] = 0xF0;
    p[5] = 0xAA;
    p[6] = 0xC8;
    if (Sc()[2] != 2) return;
    p[4] = static_cast<unsigned char>(0xF0 - Sc()[0xA] * 20);
    p[5] = static_cast<unsigned char>(0xAA - Sc()[0xA] * 14);
    p[6] = static_cast<unsigned char>(0xC8 - (Sc()[0xA] << 4));
}

}  // namespace

// original 0x4D6ED0: the ring - up to 32 flat lines round a circle of the
// radius in the scratch dword, the first +9 - 1 of them (a line i drawn while
// +9 is above i), semi-transparent in the fade; a draw mode (0x35) before and
// one (0x15) after, slot 5. Each line starts where the last drawn one ended.
C1_EXPORT void __cdecl PentagramRing_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SD(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(4, 0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SD(0))));
    for (int i = 1, angle = 0x80; angle < 0x1080; ++i, angle += 0x80) {
        if (static_cast<int>(Sc()[9]) <= i) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        if (Sc()[2] == 2) MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0, static_cast<unsigned>(Mul12(v, SD(0))));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(2, static_cast<unsigned>(Mul12(v, SD(0))));
        SetVW(4, 0);
        Rtp1(0, p + 0x14);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        LineColour(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x20);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D7080: a call through PentagramStar_Phases (three entries) by
// +2; while +0 is set, the figure under the actor's matrix.
C1_EXPORT void __cdecl PentagramStar_Run(void) {
    CellCall(Addr(PentagramStar_Phases), 3, Sc()[2], "PentagramStar_Run");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::PentagramStar_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D70B0: +9 up; at 0x28 the owner's +0xB 4 and +2 on.
C1_EXPORT void __cdecl PentagramStar_Grow(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x28) return;
    Owner()[0xB] = 4;
    Inc(Sc()[2]);
}

// original 0x4D7130: the figure - five flat lines between the six points of
// radius 0x120 at the angles PentagramStar_Points[i] x 0x111, each drawn
// (+9 - 8 i) / 8 of its length (up to all of it) once +9 passes 8 i; the
// ring's colours; draw modes 0x35 / 0x15, slot 5.
C1_EXPORT void __cdecl PentagramStar_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const auto* const points = reinterpret_cast<const signed char*>(Mem(Addr(PentagramStar_Points)));
    for (int i = 0; i < 5; ++i) {
        const int n = Sc()[9];
        if (8 * i >= n) continue;
        int len = n - 8 * i;
        if (len >= 8) len = 8;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        if (Sc()[2] == 2) MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int v = MH_CALL(Math_Sin)(points[i] * 0x111);
        SetVW(0, static_cast<unsigned>(Mul12(v, 288)));
        v = MH_CALL(Math_Cos)(points[i] * 0x111);
        SetVW(2, static_cast<unsigned>(Mul12(v, 288)));
        SetVW(4, 0);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        v = MH_CALL(Math_Sin)(points[i + 1] * 0x111);
        SetVW(8, static_cast<unsigned>(Mul12(v, 288)));
        v = MH_CALL(Math_Cos)(points[i + 1] * 0x111);
        const int c = Mul12(v, 288);
        SetVW(8, VW(0) + static_cast<unsigned>((VS(8) - VS(0)) / 8 * len));
        SetVW(0xA, VW(2) + static_cast<unsigned>((static_cast<short>(c) - VS(2)) / 8 * len));
        SetVW(0xC, 0);
        Rtp1(8, p + 0x14);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        LineColour(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x20);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D7380: a call through PentagramBand_Phases (three entries) by
// +2; while +0 is set, the band under the actor's matrix.
C1_EXPORT void __cdecl PentagramBand_Run(void) {
    CellCall(Addr(PentagramBand_Phases), 3, Sc()[2], "PentagramBand_Run");
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::PentagramBand_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D73B0: +9 up; at 0x31 the owner's +0xB 5 and +2 on.
C1_EXPORT void __cdecl PentagramBand_Grow(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x31) return;
    Owner()[0xB] = 5;
    Inc(Sc()[2]);
}

// original 0x4D73E0: once the owner's +0xB is 0x84, sound 0x103 and +2 on.
C1_EXPORT void __cdecl PentagramBand_WaitRelease(void) {
    if (Owner()[0xB] != 0x84) return;
    MH_CALL(Sound_PlayEffect)(0x103);
    Inc(Sc()[2]);
}

// original 0x4D7410: +0xA up; at 0x10 the owner's +0xB down and the task freed.
C1_EXPORT void __cdecl PentagramBand_Fade(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0x10) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D7440: the band - up to 32 gouraud quads round a circle of
// radius 0x130, standing from the ground (height +0xA x 8) to the height in
// the scratch dword +8 below it: (+9 - i) x 32 for the last sixteen drawn,
// else 0x200 + Rand & 0x3F (0x200 exactly at i 32); a quad i drawn while +9
// is above i, each sorted at its far rim point; semi-transparent, black at the
// foot, (0xC0 - +0xA x 12, (0x10 - +0xA) x 2, same) at the top. Draw mode
// 0x35 before each, 0x15 after them all (slot 3).
C1_EXPORT void __cdecl PentagramBand_Draw(void) {
    {
        const unsigned b = Sc()[9];
        SetSD(8, b < 0x10 ? b << 5 : 0x200);
    }
    int v = MH_CALL(Math_Cos)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, 304)));
    v = MH_CALL(Math_Sin)(0);
    {
        const std::uint32_t top = static_cast<std::uint32_t>(SD(8));
        SetVW(0xA, static_cast<unsigned>(Mul12(v, 304)));
        SetVW(0xC, (Sc()[0xA] << 3) - top);
    }
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, 304)));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, 304)));
    for (int i = 1; i < 0x21; ++i) {
        const int n = Sc()[9];
        if (i >= n) continue;
        const int d = n - i;
        if (d < 0x10) {
            SetSD(8, static_cast<std::uint32_t>(d) << 5);
        } else {
            const std::uint32_t r = RandCall();
            SetSD(8, (r & 0x3F) + 0x200);
            if (i == 0x20) SetSD(8, 0x200);
        }
        SetVW(0, VW(8));
        SetVW(2, VW(0xA));
        SetVW(4, VW(0xC));
        SetVW(0x10, VW(0x18));
        SetVW(0x12, VW(0x1A));
        SetVW(0x14, 0);
        const int angle = i << 7;
        v = MH_CALL(Math_Cos)(angle);
        SetVW(8, static_cast<unsigned>(Mul12(v, 304)));
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, 304)));
        SetVW(0xC, (Sc()[0xA] << 3) - static_cast<std::uint32_t>(SD(8)));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, 304)));
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0x1C, 0);
        const short zr = static_cast<short>(Mul12(v, 304));
        SetVW(0x1A, static_cast<unsigned short>(zr));
        const unsigned char* const s = Sc();
        const std::uint32_t x = (static_cast<std::uint32_t>(static_cast<int>(VS(0x18))) << 9) + static_cast<std::uint32_t>(Long(s + 0x34));
        const std::uint32_t z = (static_cast<std::uint32_t>(static_cast<int>(zr)) << 9) + static_cast<std::uint32_t>(Long(s + 0x38));
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSD(4, (0x10u - Sc()[0xA]) << 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = static_cast<unsigned char>(0xC0 - Sc()[0xA] * 12);
        p[0x25] = SB(4);
        p[0x26] = SB(4);
        p[0x34] = static_cast<unsigned char>(0xC0 - Sc()[0xA] * 12);
        p[0x35] = SB(4);
        p[0x36] = SB(4);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4D7760: a draw mode (0xB5, slot 3); with the frame-offset table
// the effects' (0x8E3580), a call through PentagramSprite_Phases (three
// entries: MAGIC130's count-down 0x4E47F0, Start, Animate) by +2; the table
// back (0x8B3580); a draw mode (0x95, slot 3).
C1_EXPORT void __cdecl PentagramSprite_Run(void) {
    DrawMode(0xB5);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    SetLong(Mem(kFrameSet), 0x8E3580);
    CellCall(Addr(PentagramSprite_Phases), 3, Sc()[2], "PentagramSprite_Run");
    SetLong(Mem(kFrameSet), 0x8B3580);
    DrawMode(0x95);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4D77D0: the sprite raised 0x180 (+0xB 2) and 0x200 more (+4 set)
// in +0x3C's high word; its sprite fields set up (+0x25 0x1D, +0x26 0, +0x27
// 0xA0, +0x28 0, +0 bit 0x20, colour (0xC0, 0xC0, 0xC0) with +0x5C 1, +0x2A
// its +4, +0x29 and +0x24 4, +0x2C 0, +0x2B 1), +9 0, +2 on; sound 0x100 for
// +0xB 0; animation +0xB.
C1_EXPORT void __cdecl PentagramSprite_Start(void) {
    unsigned char* s = Sc();
    if (s[0xB] == 2) {
        SetLong(s + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x3C)) + 0x1800000u));
        s = Sc();
    }
    if (s[4] != 0) {
        SetLong(s + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x3C)) + 0x2000000u));
        s = Sc();
    }
    s[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0] |= 0x20;
    Sc()[0x5D] = 0xC0;
    Sc()[0x5E] = 0xC0;
    Sc()[0x5F] = 0xC0;
    Sc()[0x5C] = 1;
    Sc()[0x2A] = Sc()[4];
    Sc()[0x29] = 4;
    Sc()[0x24] = 4;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 1;
    Sc()[9] = 0;
    Inc(Sc()[2]);
    if (Sc()[0xB] == 0) MH_CALL(Sound_PlayEffect)(0x100);
    MH_CALL(Sprite_SetAnimation)(Sc()[0xB]);
}

// original 0x4D78D0: the sprite's script ticked; when it reports an end, +9
// up - past 2 the owner's +0xB up and the task freed; else +0x2A (+0x2A - 1)
// & 1, the animation +0xB again, the screen update, and for +0xB 0 sound
// 0x104 (+9 1) or 0x100. While the script runs, the screen update only.
C1_EXPORT void __cdecl PentagramSprite_Animate(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) {
        MH_CALL(Sprite_UpdateScreen)();
        return;
    }
    Inc(Sc()[9]);
    unsigned char* s = Sc();
    if (s[9] > 2) {
        Inc(Owner()[0xB]);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    s[0x2A] = static_cast<unsigned char>((s[0x2A] - 1) & 1);
    MH_CALL(Sprite_SetAnimation)(Sc()[0xB]);
    MH_CALL(Sprite_UpdateScreen)();
    s = Sc();
    if (s[0xB] != 0) return;
    MH_CALL(Sound_PlayEffect)(s[9] == 1 ? 0x104 : 0x100);
}

// ===========================================================================
// MAGIC145 (row 149) and MAGIC146 (row 150)

// original 0x4E9A70: the kind-2 task. A three-entry stack table by +1:
// Ink_Start, MAGIC009's wait for +0xB 0 (0x49DA50), the engine's end
// (0x43F460).
C1_EXPORT void __cdecl Ink_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Ink_Start, kWaitChildren, kEndFlag40};
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
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(self)));
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

namespace {

// The puffs' start: at the base record's position and direction, offset by
// the (dx, dz, dy) row +0xB of `offsets` (12-byte rows, the index unbounded),
// dx / dz turned by the direction (0x446770); the screen point; +9 and +0xA 0;
// +2 on. The base is read once; what it holds is read after the turn.
void PuffPlace(const unsigned char* base, std::uint32_t offsets) {
    SetLong(Sc() + 0xC, Long(Mem(offsets + 12u * Sc()[0xB])));
    SetLong(Sc() + 0x10, Long(Mem(offsets + 4 + 12u * Sc()[0xB])));
    MH_AT(PtrFn, kTurnOffset)(Sc());
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sc() + 0xC)) +
                                                   static_cast<std::uint32_t>(Long(base + 0x34))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sc() + 0x10)) +
                                                   static_cast<std::uint32_t>(Long(base + 0x38))));
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Mem(offsets + 8 + 12u * Sc()[0xB]))) +
                                                   static_cast<std::uint32_t>(Long(base + 0x3C))));
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

}  // namespace

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
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(Sc() + 0x34)), static_cast<unsigned long>(Long(Sc() + 0x38)), 2,
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
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0x20, 0x1FA));
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
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(Sc() + 0x34)), static_cast<unsigned long>(Long(Sc() + 0x38)), 2,
                                0x48);
}

namespace {

// A pool of 64 records of 0x84 bytes (MAGIC146's, MAGIC213's), run as tasks:
// every record with bit 0 becomes Sprite_Current, its +0x80 the owner, for
// `run`; both put back after each. The task and owner are read after the
// phase call that precedes the walk.
void WalkPool(std::uint32_t pool, std::uint32_t run) {
    unsigned char* const self = Sprite_Current;
    const std::uint32_t owner = static_cast<std::uint32_t>(Long(Mem(at::kOwner)));
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(run);
        SetLong(Mem(at::kOwner), static_cast<std::int32_t>(owner));
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
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(self)));
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
        const unsigned idx = MH_AT(ByteFn, alloc)();
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

constexpr std::uint32_t kInkPool = 0x6A6F40;   // InkInkPuff_Pool
constexpr std::uint32_t kMotePool = 0x6B2958;  // Magic213Mote_Pool

}  // namespace

// original 0x4E9EF0: the kind-2 task. A three-entry stack table by +1 -
// InkInk_Start, MAGIC009's wait (0x49DA50), the engine's end (0x43F460) -
// then the puff pool walked (InkInkPuff_Dispatch).
C1_EXPORT void __cdecl InkInk_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::InkInk_Start, kWaitChildren, kEndFlag40};
    StackCall(kPhases, 3, Sc()[1], "InkInk_Task");
    WalkPool(kInkPool, bof3::addr::InkInkPuff_Dispatch);
}

// original 0x4E9F70: the puff pool cleared; +0xB 0, +9 8, +1 on; a child
// (kind 1, parameter 0x6D) on each living actor of the target's side; row 26
// as Ink_Start's (entries 1..15 with STP, entry 0 cleared), Gfx_ClutStripDirty.
C1_EXPORT void __cdecl InkInk_Start(void) {
    ClearPool(kInkPool);
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
    SetLong(Sc() + 0x34, Long(rec + 0x34));
    SetLong(Sc() + 0x38, Long(rec + 0x38));
    SetLong(Sc() + 0x3C, Long(rec + 0x3C));
    Sc()[0xB] = 0;
    Inc(Sc()[2]);
    SpawnRecords(bof3::addr::InkInkPuff_Alloc, kInkPool, 1, 2, 7);
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
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    SetLong(Sc() + 0xC, Long(Mem(Addr(InkInkPuff_Offsets) + 12u * Sc()[0xB])));
    SetLong(Sc() + 0x10, Long(Mem(Addr(InkInkPuff_Offsets) + 4 + 12u * Sc()[0xB])));
    MH_AT(PtrFn, kTurnOffset)(Sc());
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sc() + 0x34)) +
                                                   static_cast<std::uint32_t>(Long(Sc() + 0xC))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sc() + 0x38)) +
                                                   static_cast<std::uint32_t>(Long(Sc() + 0x10))));
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sc() + 0x3C)) +
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
    Call0(kFreeRecord);
}

// original 0x4EA3F0: the puff pool's alloc (0x6A6F40, 64 records).
C1_EXPORT unsigned char __cdecl InkInkPuff_Alloc(void) { return PoolAlloc(kInkPool); }

// ===========================================================================
// MAGIC213 (row 148)

// original 0x4F4A60: the kind-2 task. A two-entry stack table by +1 -
// Magic213_Start, MAGIC131's end once +0xB is 0 (0x4E5200) - then the mote
// pool walked (Magic213Mote_Dispatch).
C1_EXPORT void __cdecl Magic213_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic213_Start, kEndNoChildren};
    StackCall(kPhases, 2, Sc()[1], "Magic213_Task");
    WalkPool(kMotePool, bof3::addr::Magic213Mote_Dispatch);
}

// original 0x4F4AE0: the mote pool cleared; +0xB 0, +1 on; a child (kind 1,
// parameter 0x6B) on each living actor of the target's side (+0xB not
// written); row 26's first sixteen CLUT entries from their source, as they
// are; Gfx_ClutStripDirty; sound 0x100.
C1_EXPORT void __cdecl Magic213_Start(void) {
    ClearPool(kMotePool);
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
    if (s[9] != 0) return;
    unsigned char* const rec = ActorRecord((TargetByte() & 0x40) != 0, s[4]);
    SetLong(s + 0x34, Long(rec + 0x34));
    SetLong(Sc() + 0x38, Long(rec + 0x38));
    SetLong(Sc() + 0x3C, Long(rec + 0x3C));
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
    SpawnRecords(bof3::addr::Magic213Mote_Alloc, kMotePool, 1, 4, 0x1D);
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
    SetLong(Mem(kFrameSet), 0x8E3580);
    CellCall(Addr(Magic213Mote_Phases), 2, phase, "Magic213Mote_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), 0x8B3580);
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
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x34)) +
                                                   static_cast<std::uint32_t>(v) * 12u));
    v = MH_CALL(Math_Cos)(static_cast<short>(SW(0)));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x38)) +
                                                   static_cast<std::uint32_t>(v) * 12u));
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) + 0x1800000u));
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
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[0xB] % 3));
    Inc(Sc()[2]);
}

// original 0x4F5030: the mote's script ticked; when it reports an end, the
// owner's +0xB down and the record freed (0x4F6290).
C1_EXPORT void __cdecl Magic213Mote_End(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Dec(Owner()[0xB]);
    Call0(kFreeRecord);
}

// original 0x4F5050: the mote pool's alloc (0x6B2958, 64 records).
C1_EXPORT unsigned char __cdecl Magic213Mote_Alloc(void) { return PoolAlloc(kMotePool); }

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
    BOF3_INJECT(Pentagram_Task);
    BOF3_INJECT(Pentagram_Start);
    BOF3_INJECT(Pentagram_Rings);
    BOF3_INJECT(Pentagram_Star);
    BOF3_INJECT(Pentagram_Band);
    BOF3_INJECT(Pentagram_Sprites);
    BOF3_INJECT(Pentagram_WaitSprites);
    BOF3_INJECT(Pentagram_WaitChildren);
    BOF3_INJECT(Pentagram_DrawDisc);
    BOF3_INJECT(PentagramChild_Task);
    BOF3_INJECT(PentagramRing_Run);
    BOF3_INJECT(PentagramRing_Grow);
    BOF3_INJECT(PentagramRing_Draw);
    BOF3_INJECT(PentagramStar_Run);
    BOF3_INJECT(PentagramStar_Grow);
    BOF3_INJECT(PentagramFx_WaitRelease);
    BOF3_INJECT(PentagramFx_Fade);
    BOF3_INJECT(PentagramStar_Draw);
    BOF3_INJECT(PentagramBand_Run);
    BOF3_INJECT(PentagramBand_Grow);
    BOF3_INJECT(PentagramBand_WaitRelease);
    BOF3_INJECT(PentagramBand_Fade);
    BOF3_INJECT(PentagramBand_Draw);
    BOF3_INJECT(PentagramSprite_Run);
    BOF3_INJECT(PentagramSprite_Start);
    BOF3_INJECT(PentagramSprite_Animate);
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
