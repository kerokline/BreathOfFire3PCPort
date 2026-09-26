// Two spell overlays compiled into the exe, round nine group S30
// (docs/magic_s30.md): the PSX's MAGIC130 and MAGIC131.EMI, Magic_Rows rows
// 141 and 144. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Venom and KaiserBreath; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC130 0x4E4420..0x4E4C45: a kind-2 task at the side's centre, one
//     sprite child (a sprite that shades up, holds and shades down) and six
//     ring children (a gouraud fan and band round a screen point), CLUT row
//     26 given its stp bit; three of its functions are shared with MAGIC126
//     and MAGIC129, and its count-down 0x4E47F0 with eight overlays;
//   - MAGIC131 0x4E4C50..0x4E6944: a kind-2 task of eight steps that loads a
//     file by 0x904B89, hides the party's sprites, reloads the party set's
//     files and shows the party again; four kinds of kind-1 child (a sprite
//     that glides in and out with a cycling CLUT, a full-screen flash, a
//     textured pillar, a textured ring) and a pool of 48 motes of its own
//     (0x6A4E38, 0x2C bytes each) drawn as gouraud fans; its end 0x4E5200 is
//     shared with eighteen overlays.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack or .data dispatch table aborts where the original would call through
// whatever follows it (docs/magic_fx_reached.md section 3, the precedent), and
// Kaiser_LoadSetFile aborts where the original reads a file index through a
// null table entry (docs/magic_s30.md section 8).
#include "game/magic_s30.h"

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

// The scratch the overlays keep their working values in: sixteen bytes at
// 0x903850 (words; Scratch_Swap is +0xC) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;
// The dword the sprite children point at a sprite bank while their phase
// runs (0x8E3580 / 0x813580), and put back to 0x8B3580 after.
constexpr std::uint32_t kSpriteBank = 0x9039D8;
// MAGIC131's mote pool: 48 records of 0x2C bytes, and the cell holding the
// mote being run (KaiserMote_Pool, KaiserMote_Current).
constexpr std::uint32_t kMotePool = 0x6A4E38;
constexpr std::uint32_t kMoteStride = 0x2C;
constexpr unsigned kMotes = 48;
constexpr std::uint32_t kMoteCurrent = 0x6A5678;
// The battle bytes MAGIC131 reads beyond the harness's: 0x904AAC (the
// battle's facing, battle region), 0x904B89 (read by the engine as the
// form index, docs/magic_lib.md), 0x90412C (the loaded party set,
// docs/field-event.md), and the table of file-index tables by 0x904B89.
constexpr std::uint32_t kFacing = 0x904AAC;
constexpr std::uint32_t kForm = 0x904B89;
constexpr std::uint32_t kPartySet = 0x90412C;
constexpr std::uint32_t kSetFiles = 0x64E9BC;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Cur() { return Pointer(kMoteCurrent); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* a) { return static_cast<short>(Word(a)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddW(unsigned char* a, unsigned v) { SetWord(a, (Word(a) + v) & 0xFFFF); }
void AddL(unsigned char* a, std::int32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + static_cast<std::uint32_t>(v)));
}
// A data item's address (symbols.toml's macros are typed pointers).
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::int32_t PtrValue(const void* p) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)));
}

// `imul` then `sar 0xC` (or `sar 3`): the 32-bit product wraps, the shift is
// arithmetic.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}
int Mul12(int a, int b) { return MulSar(a, b, 12); }
// `shl eax, 6` then `sar eax, 3`.
int Shl6Sar3(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) << 6) >> 3; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* a, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(a, &f, sizeof f);
}
// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps. As
// battle_items.cpp's and field_frame.cpp's.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* Mote(unsigned i) { return Mem(kMotePool + i * kMoteStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
void DrawMode(unsigned char* prim, unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(prim, 0, 1, tpage, 0); }
void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

// This group's functions called directly, as the originals call them: by
// address, which in the game is the jmp Inject put there (or Capcom's code
// under BOF3X_ORIGINAL) and in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using AllocFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// The projections with the arguments the originals push: Gte_RotTransPers4
// gets the depth and flag pointers, Gte_RotTransPers the flag pointer too.
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S30_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp4(unsigned char* prim) {
    long depth, flag;
    S30_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &depth, &flag);
}

// MAGIC226/227's handler Venom_Task's stack table holds (group S38's).
constexpr std::uint32_t kMagic226Phase = 0x4F9F70;

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A .data dispatch table read in place (the fuzz swaps its entries for
// recorders), the index checked against the table's length.
magic_harness::Handler Entry(const char* who, std::uint32_t table, unsigned entries, unsigned index) {
    if (index >= entries) PastTable(who, index, entries);
    return reinterpret_cast<magic_harness::Handler>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Gfx_ClutStrip / Gfx_ClutStripSource word indices: row 26 (0x812980 /
// 0x80E980), row 1 (0x80F980 / 0x80B980) and row 1 + 0x21 (0x80F9C2).
constexpr unsigned kRow26 = 0x1A00, kRow1 = 0x200;

// The sprite-control fields both sprite children's starts write, in their
// order (no call between: the order is the originals' for reading only).
void SpriteFields(unsigned char c25, unsigned char c27, unsigned char shade, unsigned char c5c) {
    Sc()[0x25] = c25;
    Sc()[0x26] = 0;
    Sc()[0x27] = c27;
    Sc()[0x28] = 1;
    Sc()[0x24] = 0x80;
    SetLong(Sc() + 0x40, 0x10000);
    SetLong(Sc() + 0x44, 0x10000);
    Sc()[0x48] = 0;
    Sc()[0] = static_cast<unsigned char>(Sc()[0] | 0x20);
    Sc()[0x5C] = c5c;
    Sc()[0x5D] = shade;
    Sc()[0x5E] = shade;
    Sc()[0x5F] = shade;
}

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC130 (row 141, Venom read one id down)

// original 0x4E4420: the kind-2 task. A three-entry stack table by +1:
// Venom_Start, MAGIC226/227's 0x4F9F70, BattleFx_Finish.
S30_EXPORT void __cdecl Venom_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Venom_Start, kMagic226Phase, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Venom_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E4450: the task at the side's centre, the owner's direction
// byte, +0xB 0, +9 0x1E, +1 on; one kind-1 child 0x66 of kind 0 (the sprite)
// and six of kind 1 (the rings, +0xB their index), each with +0x80 this task
// and +0xB counting them; CLUT row 26 from its source with the stp bit, but
// its first entry without; sounds 0x100 and 0x101.
S30_EXPORT void __cdecl Venom_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[8] = Owner()[8];
    Sc()[0xB] = 0;
    Sc()[9] = 0x1E;
    Inc(Sc()[1]);
    {
        const unsigned slot = NewTask(0x66);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, PtrValue(self));
        child[1] = 0;
        Inc(self[0xB]);
    }
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = NewTask(0x66);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, PtrValue(self));
        child[1] = 1;
        child[0xB] = static_cast<unsigned char>(i);
        Inc(self[0xB]);
    }
    for (unsigned k = 0; k < 0x100; ++k)
        Gfx_ClutStrip[kRow26 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow26 + k] | 0x8000);
    Gfx_ClutStrip[kRow26] = Gfx_ClutStripSource[kRow26];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Sound_PlayById)(0x101);
}

// original 0x4E4540: the children's kind-1 task, a jmp through
// Venom_ChildKinds (two entries: the sprite, the ring) by +1.
S30_EXPORT void __cdecl VenomChild_Task(void) {
    Entry("VenomChild_Task", Addr(Venom_ChildKinds), 2, Sc()[1])();
}

// original 0x4E4560 (shared with MAGIC126 and MAGIC129): the sprite bank
// 0x9039D8 at 0x8E3580 around its phase (+2 through VenomSprite_Phases, four
// entries); Sprite_UpdateScreen while +0 bit 0 and +2 are set.
S30_EXPORT void __cdecl VenomSprite_Run(void) {
    SetLong(Mem(kSpriteBank), 0x8E3580);
    Entry("VenomSprite_Run", Addr(VenomSprite_Phases), 4, Sc()[2])();
    const unsigned char* const sc = Sc();
    if ((sc[0] & 1) != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kSpriteBank), 0x8B3580);
}

// original 0x4E45A0: the sprite at screen (0xDC, 0x6E), its control fields,
// shade 0x80, animation 0; +9 0x3C, +0xA 0xFF, +2 on.
S30_EXPORT void __cdecl VenomSprite_Start(void) {
    SetWord(Sc() + 0x2E, 0xDC);
    SetWord(Sc() + 0x30, 0x6E);
    SpriteFields(0x1D, 0x1A, 0x80, 1);
    Sc()[0x2A] = 0;
    Sc()[0x29] = 3;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0x3C;
    Sc()[0xA] = 0xFF;
    Inc(Sc()[2]);
}

// original 0x4E4690: the shade +0x5D..+0x5F up by 8; on once +0x5D is 0.
S30_EXPORT void __cdecl VenomSprite_ShadeUp(void) {
    AddB(Sc()[0x5D], 8);
    AddB(Sc()[0x5E], 8);
    AddB(Sc()[0x5F], 8);
    if (Sc()[0x5D] == 0) Inc(Sc()[2]);
}

// original 0x4E46D0: the shade down by 8; at +0x5D 0x80 the owner's count
// down and the task freed.
S30_EXPORT void __cdecl VenomSprite_ShadeDown(void) {
    AddB(Sc()[0x5D], 0xF8);
    AddB(Sc()[0x5E], 0xF8);
    AddB(Sc()[0x5F], 0xF8);
    if (Sc()[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E4720: +2 through VenomRing_Phases (four entries: start, grow,
// MagicFx_CountDown9, BlizzardShard_End); a draw mode (tpage 0x35) committed;
// then, while +0 and +2 are set, the fan and the band.
S30_EXPORT void __cdecl VenomRing_Run(void) {
    Entry("VenomRing_Run", Addr(VenomRing_Phases), 4, Sc()[2])();
    DrawMode(Gfx_PacketNext, 0x35);
    Commit(2, 0xC);
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    Call0(bof3::addr::VenomRing_DrawFan);
    Call0(bof3::addr::VenomRing_DrawBand);
}

// original 0x4E4770: the ring at screen x +0xB x 26 + 0x98 (a word),
// y VenomRing_ScreenY[+0xB] + 0x50 (+0xB unbounded); radius +0x14 0x20, +9
// 0x3C, +0xA 0, +2 on.
S30_EXPORT void __cdecl VenomRing_Start(void) {
    const unsigned b = Sc()[0xB];
    SetWord(Sc() + 0x2E, (b * 26 + 0x98) & 0xFFFF);
    SetWord(Sc() + 0x30, (Word(Mem(Addr(VenomRing_ScreenY) + 2 * Sc()[0xB])) + 0x50) & 0xFFFF);
    SetLong(Sc() + 0x14, 0x20);
    Sc()[9] = 0x3C;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4E47D0: +0xA up; on at 0x10.
S30_EXPORT void __cdecl VenomRing_Grow(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4E47F0 (a phase of eight overlays' tables): +9 down; on at 0.
S30_EXPORT void __cdecl MagicFx_CountDown9(void) {
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4E4810: a fan of eight gouraud triangles round the screen point
// (+0x2E, +0x30), radius +0x14; the centre's shade +0xA x 8, the rim's
// (+0xA x 8, x 4, x 8) - the scratch words +4..+0xA.
S30_EXPORT void __cdecl VenomRing_DrawFan(void) {
    {
        const unsigned char* const sc = Sc();
        SetSW(0, Word(sc + 0x14));
        SetSW(4, sc[0xA] << 3);
        SetSW(6, sc[0xA] << 3);
        SetSW(8, sc[0xA] << 2);
        SetSW(0xA, sc[0xA] << 3);
    }
    int s = Sin(0);
    SetVW(0, static_cast<unsigned>(Mul12(s, SS(0)) + Word(Sc() + 0x2E)));
    int c = Cos(0);
    SetVW(2, static_cast<unsigned>(Mul12(c, SS(0)) + Word(Sc() + 0x30)));
    for (int angle = 0x200; angle < 0x1200; angle += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        PutFloat(p + 0x18, VS(0));
        PutFloat(p + 0x1C, VS(2));
        s = Sin(angle);
        SetVW(0, static_cast<unsigned>(Mul12(s, SS(0)) + Word(Sc() + 0x2E)));
        PutFloat(p + 0x28, VS(0));
        c = Cos(angle);
        SetVW(2, static_cast<unsigned>(Mul12(c, SS(0)) + Word(Sc() + 0x30)));
        PutFloat(p + 0x2C, VS(2));
        p[4] = SB(4);
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = SB(6);
        p[0x15] = SB(8);
        p[0x16] = SB(0xA);
        p[0x24] = SB(6);
        p[0x25] = SB(8);
        p[0x26] = SB(0xA);
        Commit(2, 0x34);
    }
}

// original 0x4E49F0: a band of eight gouraud quads round the same point
// between radius +0x14 and +0x14 x 2 + Rand & 7; the outer edge's shade 1,
// the inner's the fan's rim (scratch +6..+0xA).
S30_EXPORT void __cdecl VenomRing_DrawBand(void) {
    SetSW(0, Word(Sc() + 0x14));
    const std::uint32_t r = RandCall();
    SetSW(2, (r & 7) + (static_cast<unsigned>(Word(Sc() + 0x14)) << 1));
    int v = Sin(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SS(2)) + Word(Sc() + 0x2E)));
    v = Cos(0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(2)) + Word(Sc() + 0x30)));
    v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0)) + Word(Sc() + 0x2E)));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0)) + Word(Sc() + 0x30)));
    for (int angle = 0x200; angle < 0x1200; angle += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, VS(0));
        PutFloat(p + 0xC, VS(2));
        PutFloat(p + 0x28, VS(8));
        PutFloat(p + 0x2C, VS(0xA));
        v = Sin(angle);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2)) + Word(Sc() + 0x2E)));
        PutFloat(p + 0x18, VS(0));
        v = Cos(angle);
        SetVW(2, static_cast<unsigned>(Mul12(v, SS(2)) + Word(Sc() + 0x30)));
        PutFloat(p + 0x1C, VS(2));
        v = Sin(angle);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0)) + Word(Sc() + 0x2E)));
        PutFloat(p + 0x38, VS(8));
        v = Cos(angle);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0)) + Word(Sc() + 0x30)));
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        PutFloat(p + 0x3C, VS(0xA));
        p[0x24] = SB(6);
        p[0x25] = SB(8);
        p[0x26] = SB(0xA);
        p[0x34] = SB(6);
        p[0x35] = SB(8);
        p[0x36] = SB(0xA);
        Commit(2, 0x44);
    }
}

// ===========================================================================
// MAGIC131 (row 144, KaiserBreath read one id down)

// original 0x4E4C50: the kind-2 task. An eight-entry stack table by +1 (Start,
// LoadFormFile, HideParty, WaitChildren, ReloadParty, LoadSetFile, ShowParty,
// MagicFx_EndWhenChildrenDone); a draw mode (tpage 0x55) committed; every
// live mote run (KaiserMote_Current the record, the owner its +0x28, the
// owner read before the walk put back after each); a draw mode (tpage 0x15)
// committed.
S30_EXPORT void __cdecl Kaiser_Task(void) {
    static constexpr std::uint32_t kPhases[8] = {
        bof3::addr::Kaiser_Start,       bof3::addr::Kaiser_LoadFormFile, bof3::addr::Kaiser_HideParty,
        bof3::addr::Kaiser_WaitChildren, bof3::addr::Kaiser_ReloadParty, bof3::addr::Kaiser_LoadSetFile,
        bof3::addr::Kaiser_ShowParty,   bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sc()[1];
    if (phase >= 8) PastTable("Kaiser_Task", phase, 8);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(Gfx_PacketNext, 0x55);
    Commit(2, 0xC);
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) == 0) continue;
        SetLong(Mem(kMoteCurrent), PtrValue(m));
        SetLong(Mem(at::kOwner), Long(m + 0x28));
        Call0(bof3::addr::KaiserMote_Task);
        SetLong(Mem(at::kOwner), owner);
    }
    DrawMode(Gfx_PacketNext, 0x15);
    Commit(2, 0xC);
}

// original 0x4E4D20: every mote's +0..+2 cleared; the owner's direction
// byte; the task at the side's centre; +0xB 0, +9 0xA, +0xA 0, +1 on; one
// kind-1 child 0x69 of kind 1 (the flash); CLUT row 26 from its source with
// the stp bit; sound 0x102.
S30_EXPORT void __cdecl Kaiser_Start(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        Mote(i)[0] = 0;
        Mote(i)[1] = 0;
        Mote(i)[2] = 0;
    }
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0xA;
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x69);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, PtrValue(self));
    child[1] = 1;
    Inc(self[0xB]);
    for (unsigned k = 0; k < 0x100; ++k)
        Gfx_ClutStrip[kRow26 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow26 + k] | 0x8000);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x102);
}

// original 0x4E4DF0: +9 down; at 0 LoadDatFile(0x228 when 0x904B89 is 7,
// 0x227 when 8, else 0x229) and +1 on.
S30_EXPORT void __cdecl Kaiser_LoadFormFile(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned form = Mem(kForm)[0];
    MH_CALL(LoadDatFile)(form == 7 ? 0x228 : form == 8 ? 0x227 : 0x229);
    Inc(Sc()[1]);
}

// original 0x4E4E40: once File_LoadDone: the three party members' +0 cleared
// (their sprites off); a kind-1 child 0x69 of kind 0 (the sprite), +4 0x21,
// +9 0x18; CLUT row 1 copied from its source; +1 on.
S30_EXPORT void __cdecl Kaiser_HideParty(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    PartyRecord(0)[0] = 0;
    PartyRecord(1)[0] = 0;
    PartyRecord(2)[0] = 0;
    const unsigned slot = NewTask(0x69);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, PtrValue(self));
    child[1] = 0;
    child[4] = 0x21;
    child[9] = 0x18;
    Inc(self[0xB]);
    for (unsigned k = 0; k < 0x100; ++k) Gfx_ClutStrip[kRow1 + k] = Gfx_ClutStripSource[kRow1 + k];
    Gfx_ClutStripDirty = 1;
    Inc(Sc()[1]);
}

// original 0x4E4EE0: once +0xB (the children) is 0: a kind-1 child 0x69 of
// kind 1 (the flash); +9 0xA; +1 on.
S30_EXPORT void __cdecl Kaiser_WaitChildren(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned slot = NewTask(0x69);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, PtrValue(self));
    child[1] = 1;
    Inc(self[0xB]);
    Sc()[9] = 0xA;
    Inc(Sc()[1]);
}

// original 0x4E4F40: +9 down; at 0: if a member below Field_MemberCount has
// +0x89 4 and +0x134 bit 0, one file by the party set 0x90412C (7, 0xD, 0xE,
// 0xF: 0x122..0x125, or 0x126..0x129 with 0x904AAC bit 1); else
// PartySet_Select(the set & 0x7F, 2 with 0x904AAC bit 1, else 1). +1 on.
S30_EXPORT void __cdecl Kaiser_ReloadParty(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned count = Field_MemberCount;
    bool found = false;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const m = PartyRecord(i);
        if (m[0x89] == 4 && (m[0x134] & 1) != 0) found = true;
    }
    if (found) {
        const auto bit = [] { return (Mem(kFacing)[0] & 2) != 0; };
        if (Mem(kPartySet)[0] == 7) MH_CALL(LoadDatFile)(bit() ? 0x126 : 0x122);
        if (Mem(kPartySet)[0] == 0xD) MH_CALL(LoadDatFile)(bit() ? 0x127 : 0x123);
        if (Mem(kPartySet)[0] == 0xE) MH_CALL(LoadDatFile)(bit() ? 0x128 : 0x124);
        if (Mem(kPartySet)[0] == 0xF) MH_CALL(LoadDatFile)(bit() ? 0x129 : 0x125);
    } else {
        const unsigned mode = (Mem(kFacing)[0] & 2) != 0 ? 2 : 1;
        MH_CALL(PartySet_Select)(Mem(kPartySet)[0] & 0x7Fu, mode);
    }
    Inc(Sc()[1]);
}

// original 0x4E5090: once File_LoadDone: LoadDatFile(the word at
// [0x64E9BC + 0x904B89 x 8 (+4 with 0x904AAC bit 1)] + the party set x 2),
// both indices unchecked; +1 on.
S30_EXPORT void __cdecl Kaiser_LoadSetFile(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    const unsigned half = (Mem(kFacing)[0] & 2) != 0 ? 4 : 0;
    const unsigned set = Mem(kPartySet)[0];
    const unsigned form = Mem(kForm)[0];
    const std::uint32_t table = static_cast<std::uint32_t>(Long(Mem(kSetFiles + form * 8 + half)));
    // The original reads through the entry unchecked; a null one faults.
    if (table == 0)
        bof3::Fatal("Kaiser_LoadSetFile: 0x904B89 is %u, whose file table (0x%X) is null", form,
                    (unsigned)(kSetFiles + form * 8 + half));
    MH_CALL(LoadDatFile)(Word(Mem(table + set * 2)));
    Inc(Sc()[1]);
}

// original 0x4E5100: once File_LoadDone: for each member below
// Field_MemberCount (re-read each time round), Sprite_Current the member,
// +0 1 and animation +8 + 4, then + 0x18 with +0x90 & 0x2080, + 0x30 with
// +0x91 bit 3, + 0x1C when Battle_ActorIsOut; Sprite_Current put back; +1 on.
S30_EXPORT void __cdecl Kaiser_ShowParty(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    if (Field_MemberCount == 0) {
        Inc(Sc()[1]);
        return;
    }
    unsigned char* const saved = Sc();
    unsigned i = 0;
    do {
        unsigned char* const m = PartyRecord(i);
        Sprite_Current = m;
        m[0] = 1;
        MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(m[8] + 4));
        if ((Word(m + 0x90) & 0x2080) != 0) MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0x18));
        if ((m[0x91] & 8) != 0) MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0x30));
        if (MH_CALL(Battle_ActorIsOut)(i) != 0) MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0x1C));
        i = (i + 1) & 0xFF;
        Sprite_Current = saved;
    } while (i < Field_MemberCount);
    Inc(saved[1]);
}

// original 0x4E5200 (a phase of eighteen overlays' tables): once +0xB (the
// children left) is 0, the effect-done bit 2 of 0x904AA8 and the task freed.
S30_EXPORT void __cdecl MagicFx_EndWhenChildrenDone(void) {
    if (Sc()[0xB] != 0) return;
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5220: the children's kind-1 task, a jmp through
// Kaiser_ChildKinds (four entries: sprite, flash, pillar, ring) by +1.
S30_EXPORT void __cdecl KaiserChild_Task(void) {
    Entry("KaiserChild_Task", Addr(Kaiser_ChildKinds), 4, Sc()[1])();
}

// original 0x4E5240: the sprite bank at 0x813580 around its phase (+2
// through KaiserSprite_Phases, eight entries); while +0 and +2 are set,
// Sprite_UpdateScreen, and every fourth frame nine CLUT entries (row 1 from
// 0x21) from the source at row 1 + +4 with the stp bit, +4 up by 9 and back
// to 0x21 past 0x7A.
S30_EXPORT void __cdecl KaiserSprite_Run(void) {
    SetLong(Mem(kSpriteBank), 0x813580);
    Entry("KaiserSprite_Run", Addr(KaiserSprite_Phases), 8, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) {
        MH_CALL(Sprite_UpdateScreen)();
        if ((Frame_Counter & 3) == 0) {
            unsigned char* const self = Sc();
            for (unsigned i = 0; i < 9; ++i)
                Gfx_ClutStrip[kRow1 + 0x21 + i] =
                    static_cast<unsigned short>(Gfx_ClutStripSource[kRow1 + self[4] + i] | 0x8000);
            Gfx_ClutStripDirty = 1;
            AddB(self[4], 9);
            if (Sc()[4] > 0x7A) Sc()[4] = 0x21;
        }
    }
    SetLong(Mem(kSpriteBank), 0x8B3580);
}

namespace {

// The facing test the sprite's phases share: 0x904AAC neither 0 nor 3.
bool FacingAcross() {
    const unsigned f = Mem(kFacing)[0];
    return f != 0 && f != 3;
}
// One step of the sprite's glide: the velocity +0xC / +0x10 by +0x18 / +0x1C,
// the screen point by the velocity (as words).
void Accelerate() {
    AddL(Sc() + 0xC, Long(Sc() + 0x18));
    AddL(Sc() + 0x10, Long(Sc() + 0x1C));
}
void Move() {
    AddW(Sc() + 0x2E, Word(Sc() + 0xC));
    AddW(Sc() + 0x30, Word(Sc() + 0x10));
}
void SetVelocity(std::int32_t vx, std::int32_t vy, std::int32_t ax, std::int32_t ay) {
    SetLong(Sc() + 0xC, vx);
    SetLong(Sc() + 0x10, vy);
    SetLong(Sc() + 0x18, ax);
    SetLong(Sc() + 0x1C, ay);
}

}  // namespace

// original 0x4E52E0: +9 down; at 0: by the facing, the start point and
// velocity ((0x103, 0x46), (4, -4), (1, -1), +0x2A 0; or (0x3D, 0x46),
// (-4, -4), (-1, -1), +0x2A 1) run sixteen steps back and the velocity
// negated; the sprite's control fields, shade 0, animation 0; sound 0x101;
// +9 0x10, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    if (FacingAcross()) {
        SetWord(Sc() + 0x2E, 0x103);
        SetWord(Sc() + 0x30, 0x46);
        SetVelocity(4, -4, 1, -1);
        Sc()[0x2A] = 0;
    } else {
        SetWord(Sc() + 0x2E, 0x3D);
        SetWord(Sc() + 0x30, 0x46);
        SetVelocity(-4, -4, -1, -1);
        Sc()[0x2A] = 1;
    }
    for (unsigned i = 0; i < 16; ++i) {
        Accelerate();
        Move();
    }
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(Long(Sc() + 0xC))));
    SetLong(Sc() + 0x10, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(Long(Sc() + 0x10))));
    SpriteFields(5, 2, 0, 0);
    Sc()[0x29] = 2;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
    MH_CALL(Sprite_SetAnimation)(0);
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4E54D0: a step (move, then accelerate); +9 down; at 0 a new
// velocity by the facing ((-4, 4), (1, -1); or (4, 4), (-1, -1)), +9 6, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Glide(void) {
    Move();
    Accelerate();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    if (FacingAcross())
        SetVelocity(-4, 4, 1, -1);
    else
        SetVelocity(4, 4, -1, -1);
    Sc()[9] = 6;
    Inc(Sc()[2]);
}

// original 0x4E55A0: a step (accelerate, then move); +9 down; at 0 +9 0x24,
// +2 on.
S30_EXPORT void __cdecl KaiserSprite_Brake(void) {
    Accelerate();
    Move();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0x24;
    Inc(Sc()[2]);
}

// original 0x4E5600: the sprite's script ticked; +9 down; at 0 animation 1,
// +9 0x29, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Tick(void) {
    MH_CALL(Sprite_ScriptTick)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_SetAnimation)(1);
    Sc()[9] = 0x29;
    Inc(Sc()[2]);
}

// original 0x4E5640: the script ticked once; +9 down; at 0: sound 0x100, the
// target flags 0x10; two kind-1 children 0x69 of kinds 2 and 3 (pillar,
// ring) of the owner; eight motes of kind 0 (+7 0..7) and 28 of kind 1 (+7
// 0..0x1B, the delay +0x26 (k < 16 ? k / 2 & 7 : k & 0xF) + 1), each whose
// alloc answers other than 0xFF owned by the owner, its +0xB counting them;
// +2 on.
S30_EXPORT void __cdecl KaiserSprite_Breathe(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    for (unsigned kind = 2; kind <= 3; ++kind) {
        const unsigned slot = NewTask(0x69);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, PtrValue(owner));
        child[1] = static_cast<unsigned char>(kind);
        Inc(owner[0xB]);
    }
    const auto alloc = [] { return static_cast<unsigned>(MH_AT(AllocFn, bof3::addr::KaiserMote_Alloc)()); };
    for (unsigned k = 0; k < 8; ++k) {
        const unsigned r = alloc() & 0xFF;
        if (r == 0xFF) continue;
        unsigned char* const owner = Owner();
        unsigned char* const m = Mote(r);
        SetLong(m + 0x28, PtrValue(owner));
        m[1] = 0;
        m[2] = 0;
        m[7] = static_cast<unsigned char>(k);
        Inc(owner[0xB]);
    }
    for (unsigned k = 0; k < 0x1C; ++k) {
        const unsigned r = alloc() & 0xFF;
        if (r == 0xFF) continue;
        unsigned char* const owner = Owner();
        unsigned char* const m = Mote(r);
        SetLong(m + 0x28, PtrValue(owner));
        m[1] = 1;
        m[2] = 0;
        m[7] = static_cast<unsigned char>(k);
        const unsigned d = k < 0x10 ? (k >> 1) & 7 : k & 0xF;
        SetWord(m + 0x26, d + 1);
        Inc(owner[0xB]);
    }
    Inc(Sc()[2]);
}

// original 0x4E57C0: the script ticked once; when it reports its end, +9
// 0xF and +2 on.
S30_EXPORT void __cdecl KaiserSprite_WaitScript(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    Sc()[9] = 0xF;
    Inc(Sc()[2]);
}

// original 0x4E57E0: +9 down; at 0 the target flag 0x40, a velocity by the
// facing ((-8, -1), (-1, -1); or (8, -1), (1, -1)), +9 0, +0xA 0x1E, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Leave(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    if (FacingAcross())
        SetVelocity(-8, -1, -1, -1);
    else
        SetVelocity(8, -1, 1, -1);
    Sc()[9] = 0;
    Sc()[0xA] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4E5890: on odd frames while +9 is below 8, +9 up and the
// y-velocity by its acceleration; the point moved; +0xA down, at 0 the
// owner's count down and the task freed.
S30_EXPORT void __cdecl KaiserSprite_Exit(void) {
    if ((Frame_Counter & 1) != 0 && Sc()[9] < 8) {
        Inc(Sc()[9]);
        AddL(Sc() + 0x10, Long(Sc() + 0x1C));
    }
    Move();
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5900: +2 through KaiserFlash_Phases (five entries: clear,
// brighten, wait for the load, MAGIC078's MagicFx_WaitA, MAGIC060's 0x4B1740);
// while +0 and +2 are set, the flash.
S30_EXPORT void __cdecl KaiserFlash_Run(void) {
    Entry("KaiserFlash_Run", Addr(KaiserFlash_Phases), 5, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    Call0(bof3::addr::KaiserFlash_Draw);
}

// original 0x4E5930 (a phase of MAGIC060, 064 and 131): +9 0, +2 on.
S30_EXPORT void __cdecl MagicFx_ClearCount9(void) {
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4E5950 (a phase of sixteen overlays): +9 up by 2; on at 0x10.
S30_EXPORT void __cdecl MagicFx_CountUp9By2(void) {
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4E5970: once the owner's +9 is 0 and File_LoadDone, +0xA 8 and
// +2 on.
S30_EXPORT void __cdecl KaiserFlash_WaitLoad(void) {
    if (Owner()[9] != 0) return;
    if (MH_CALL(File_LoadDone)() == 0) return;
    Sc()[0xA] = 8;
    Inc(Sc()[2]);
}

// original 0x4E59A0: a semi-transparent flat quad over the screen (0..319 x
// 0..239), shade +9 x 15, between draw modes of tpage 0x35 and 0x15.
S30_EXPORT void __cdecl KaiserFlash_Draw(void) {
    DrawMode(Gfx_PacketNext, 0x35);
    Commit(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyF4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    constexpr std::int32_t kRight = 0x439F8000, kBottom = 0x436F0000;   // 319.0f, 239.0f
    SetLong(p + 8, 0);
    SetLong(p + 0xC, 0);
    SetLong(p + 0x14, kRight);
    SetLong(p + 0x18, 0);
    SetLong(p + 0x20, 0);
    SetLong(p + 0x24, kBottom);
    SetLong(p + 0x2C, kRight);
    SetLong(p + 0x30, kBottom);
    const auto shade = static_cast<unsigned char>(Sc()[9] * 15);
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    Commit(2, 0x38);
    DrawMode(Gfx_PacketNext, 0x15);
    Commit(2, 0xC);
}

// original 0x4E5A40: +2 through KaiserPillar_Phases (three entries); while +0
// and +2 are set, the pillar under the actor's matrix.
S30_EXPORT void __cdecl KaiserPillar_Run(void) {
    Entry("KaiserPillar_Run", Addr(KaiserPillar_Phases), 3, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::KaiserPillar_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E5A80: at (Field_Kind2X, Field_Kind2Z), the owner's height;
// shade +0x5D 0x10, +9 0x20, +0xA 8, +2 on.
S30_EXPORT void __cdecl KaiserPillar_Start(void) {
    SetLong(Sc() + 0x34, Field_Kind2X);
    SetLong(Sc() + 0x38, Field_Kind2Z);
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0x5D] = 0x10;
    Sc()[9] = 0x20;
    Sc()[0xA] = 8;
    Inc(Sc()[2]);
}

// original 0x4E5AE0: +9 up by 4; on at 0x40.
S30_EXPORT void __cdecl KaiserPillar_Grow(void) {
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x40) Inc(Sc()[2]);
}

// original 0x4E5B00: +9 up by 2, the shade +0x5D down by 4; at 0 the owner's
// count down and the task freed.
S30_EXPORT void __cdecl KaiserPillar_Fade(void) {
    AddB(Sc()[9], 2);
    AddB(Sc()[0x5D], 0xFC);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5B40: eight rings of sixteen textured quads (fourteen drawn:
// the 10th and 11th of each ring are skipped), each ring from radius +9 x 16
// - 0x40 j out to 0x40 more, its heights -(sin(0x100 j) x +0xA x 16 >> 12)
// of the ring before and the new; u by the quad's parity and j / 4, v and
// its height from KaiserPillar_V / _H[j]; shade the signed +0x5D x 8.
S30_EXPORT void __cdecl KaiserPillar_Draw(void) {
    DrawMode(Gfx_PacketNext, 0x35);
    Commit(3, 0xC);
    {
        const unsigned char* const sc = Sc();
        const unsigned shade = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(sc[0x5D]))) << 3;
        SetSW(8, shade);
        SetSW(0xA, shade);
        SetSW(0xC, shade);
        SetSW(0, sc[0xA] << 4);
        SetSW(2, sc[9] << 4);
    }
    int v = Sin(0);
    SetVW(0xC, static_cast<unsigned>(-Mul12(v, SS(0))));
    int outer = 0x100;
    for (unsigned j = 0; j < 8; ++j, outer += 0x100) {
        const std::uint16_t r = SW(2);
        SetSW(4, r);
        SetSW(2, r - 0x40);
        v = Sin(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Cos(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        const std::uint16_t top = VW(0xC);
        SetVW(0x14, top);
        SetVW(0x1C, top);
        v = Sin(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Sin(outer);
        const auto height = static_cast<unsigned>(-Mul12(v, SS(0)));
        SetVW(4, height);
        SetVW(0xC, height);
        const auto band = static_cast<unsigned char>((j >> 2) << 1);
        const unsigned char tv = Mem(Addr(KaiserPillar_V))[j];
        const unsigned char th = Mem(Addr(KaiserPillar_H))[j];
        for (unsigned q = 1; q < 0x11; ++q) {
            const unsigned angle = (q & 0xF) << 8;
            SetSW(6, angle);
            SetVW(0, VW(8));
            SetVW(2, VW(0xA));
            v = Sin(static_cast<int>(angle));
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = Cos(SS(6));
            const std::uint16_t x18 = VW(0x18), x1a = VW(0x1A);
            const int next = SS(6);
            SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
            SetVW(0x10, x18);
            SetVW(0x12, x1a);
            v = Sin(next);
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = Cos(SS(6));
            unsigned char* const p = Gfx_PacketNext;
            SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
            MH_CALL(Gpu_SetPolyFT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100);
            SetWord(p + 0x26, tpage & 0xFFFF);
            const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
            SetWord(p + 0x16, clut & 0xFFFF);
            const auto u = static_cast<unsigned char>(((q & 1) + band) << 6);
            p[0x14] = u;
            p[0x15] = tv;
            p[0x24] = static_cast<unsigned char>(u + 0x3F);
            p[0x25] = tv;
            p[0x34] = u;
            p[0x35] = static_cast<unsigned char>(tv + th - 1);
            p[0x44] = static_cast<unsigned char>(u + 0x3F);
            p[0x45] = static_cast<unsigned char>(tv + th - 1);
            Rtp4(p);
            MH_CALL(Gte_PrimDepths4_10)(p);
            p[4] = SB(8);
            p[5] = SB(0xA);
            p[6] = SB(0xC);
            if (q <= 9 || q >= 0xC) Commit(3, 0x48);
        }
    }
}

// original 0x4E5EA0: +2 through KaiserRing_Phases (three entries); while +0
// and +2 are set, the ring under the actor's matrix.
S30_EXPORT void __cdecl KaiserRing_Run(void) {
    Entry("KaiserRing_Run", Addr(KaiserRing_Phases), 3, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::KaiserRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E5EE0: at (Field_Kind2X, Field_Kind2Z), the owner's height;
// radius +0x14 0x40, +0xB 0, +9 0x10, +0xA 1, +2 on.
S30_EXPORT void __cdecl KaiserRing_Start(void) {
    SetLong(Sc() + 0x34, Field_Kind2X);
    SetLong(Sc() + 0x38, Field_Kind2Z);
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    SetLong(Sc() + 0x14, 0x40);
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4E5F40: the radius up by 0x20; at 0x200 +0xB 1 and +2 on.
S30_EXPORT void __cdecl KaiserRing_Grow(void) {
    AddL(Sc() + 0x14, 0x20);
    if (Long(Sc() + 0x14) != 0x200) return;
    Sc()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4E5F70: the radius up by 0x10, +9 down by 2; at 0 the owner's
// count down and the task freed.
S30_EXPORT void __cdecl KaiserRing_Fade(void) {
    AddL(Sc() + 0x14, 0x10);
    AddB(Sc()[9], 0xFE);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5FB0: a ring of sixteen textured quads (ten drawn: angles
// 0x700..0xE00 skipped) of radius +0x14 from height -0x400 to 0; the blend
// mode +0xB (the draw mode's tpage ((+0xB & 3) << 5) | 0x15); u by the quad's
// parity; shade +9 x 8.
S30_EXPORT void __cdecl KaiserRing_Draw(void) {
    {
        unsigned char* const p0 = Gfx_PacketNext;
        DrawMode(p0, ((Sc()[0xB] & 3u) << 5) | 0x15);
    }
    Commit(3, 0xC);
    {
        const unsigned char* const sc = Sc();
        SetSW(0, Word(sc + 0x14));
        SetSW(8, sc[9] << 3);
        SetSW(0xA, sc[9] << 3);
        SetSW(0xC, sc[9] << 3);
    }
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
    std::uint32_t counter = 1;
    for (int angle = 0x100; angle < 0x1100; angle += 0x100, ++counter) {
        const std::uint16_t x = VW(8), z = VW(0xA);
        SetVW(0, x);
        SetVW(2, z);
        SetVW(4, 0xFC00);
        SetVW(0x10, x);
        SetVW(0x12, z);
        SetVW(0x14, 0);
        v = Sin(angle);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(angle);
        unsigned char* const p = Gfx_PacketNext;
        const std::uint16_t nx = VW(8);
        const auto nz = static_cast<std::uint16_t>(Mul12(v, SS(0)));
        SetVW(0xA, nz);
        SetVW(0xC, 0xFC00);
        SetVW(0x18, nx);
        SetVW(0x1A, nz);
        SetVW(0x1C, 0);
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, Sc()[0xB]);
        const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, Sc()[0xB], 0x340, 0x100);
        SetWord(p + 0x26, tpage & 0xFFFF);
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        SetWord(p + 0x16, clut & 0xFFFF);
        const auto u = static_cast<unsigned char>((counter & 1) << 6);
        p[0x15] = 0;
        p[0x14] = u;
        p[0x25] = 0;
        p[0x24] = static_cast<unsigned char>(u + 0x3F);
        p[0x34] = u;
        p[0x35] = 0xBF;
        p[0x44] = static_cast<unsigned char>(u + 0x3F);
        p[0x45] = 0xBF;
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        if (angle <= 0x600 || angle >= 0xF00) Commit(3, 0x48);
    }
}

// original 0x4E6200: a mote's kind (KaiserMote_Current +1) through
// KaiserMote_Kinds (two entries), a jmp.
S30_EXPORT void __cdecl KaiserMote_Task(void) {
    Entry("KaiserMote_Task", Addr(KaiserMote_Kinds), 2, Cur()[1])();
}

namespace {

// A kind-0 or kind-1 mote's position: (sin, cos) of its angle (the scratch
// word +6, re-read for the cosine) times `scale` >> 3 about
// (Field_Kind2X, Field_Kind2Z); `scale` is 0 for the fixed 64 (<< 6).
void MotePlace(unsigned angle, bool by_radius) {
    SetSW(6, angle);
    int v = Sin(static_cast<short>(angle));
    SetLong(Cur() + 0x14, static_cast<std::int32_t>(
                              static_cast<std::uint32_t>(by_radius ? MulSar(v, S16(Cur() + 0xC), 3) : Shl6Sar3(v)) +
                              static_cast<std::uint32_t>(Field_Kind2X)));
    v = Cos(SS(6));
    SetLong(Cur() + 0x18, static_cast<std::int32_t>(
                              static_cast<std::uint32_t>(by_radius ? MulSar(v, S16(Cur() + 0xC), 3) : Shl6Sar3(v)) +
                              static_cast<std::uint32_t>(Field_Kind2Z)));
}
unsigned AngleA() { return (static_cast<unsigned>(Mem(Addr(KaiserMoteA_Angles))[Cur()[7]]) << 7) & 0xFFFF; }
unsigned AngleB() { return (static_cast<unsigned>(Mem(Addr(KaiserMoteB_Angles))[Cur()[7]]) << 6) & 0xFFFF; }

void MoteDraw() {
    Call0(bof3::addr::KaiserMote_UpdateScreenXY);
    Call0(bof3::addr::KaiserMote_Draw);
}

}  // namespace

// original 0x4E6220: +2 through KaiserMoteA_Phases (three entries); while +0
// and +2 are set, its screen point and its fan.
S30_EXPORT void __cdecl KaiserMoteA_Run(void) {
    Entry("KaiserMoteA_Run", Addr(KaiserMoteA_Phases), 3, Cur()[2])();
    const unsigned char* const c = Cur();
    if (c[0] == 0 || c[2] == 0) return;
    MoteDraw();
}

// original 0x4E6250: at radius 64 about the point, angle
// KaiserMoteA_Angles[+7] << 7 (+7 unbounded); the owner's height; +0xC 0x40,
// +5 0x10, +6 0x14, +2 on.
S30_EXPORT void __cdecl KaiserMoteA_Start(void) {
    MotePlace(AngleA(), false);
    SetLong(Cur() + 0x1C, Long(Owner() + 0x3C));
    SetWord(Cur() + 0xC, 0x40);
    Cur()[5] = 0x10;
    Cur()[6] = 0x14;
    Inc(Cur()[2]);
}

// original 0x4E62F0: +6 up by 2, the radius +0xC up by 0x20, placed; on at
// 0x200.
S30_EXPORT void __cdecl KaiserMoteA_Spread(void) {
    AddB(Cur()[6], 2);
    AddW(Cur() + 0xC, 0x20);
    MotePlace(AngleA(), true);
    if (Word(Cur() + 0xC) == 0x200) Inc(Cur()[2]);
}

// original 0x4E6390: the radius up by 0x10, placed; +5 down by 2; at 0 the
// owner's count down and the mote freed.
S30_EXPORT void __cdecl KaiserMoteA_Fade(void) {
    AddW(Cur() + 0xC, 0x10);
    MotePlace(AngleA(), true);
    AddB(Cur()[5], 0xFE);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::KaiserMote_Free);
}

// original 0x4E6430: +2 through KaiserMoteB_Phases (three entries); then
// while its delay +0x26 is not 0, the delay down; else, while +0 and +2 are
// set, its screen point and its fan.
S30_EXPORT void __cdecl KaiserMoteB_Run(void) {
    Entry("KaiserMoteB_Run", Addr(KaiserMoteB_Phases), 3, Cur()[2])();
    unsigned char* const c = Cur();
    const std::uint16_t delay = Word(c + 0x26);
    if (delay != 0) {
        SetWord(c + 0x26, (delay - 1u) & 0xFFFF);
        return;
    }
    if (c[0] == 0 || c[2] == 0) return;
    MoteDraw();
}

// original 0x4E6470: at radius 64 about the point, angle
// KaiserMoteB_Angles[+7] << 6; the owner's height, its high word up by
// KaiserMoteB_Lift[+7] << 6 (+7 unbounded); +0xC 0x40, +5 0x10, +6 8, +2 on.
S30_EXPORT void __cdecl KaiserMoteB_Start(void) {
    MotePlace(AngleB(), false);
    SetLong(Cur() + 0x1C, Long(Owner() + 0x3C));
    AddW(Cur() + 0x1E, static_cast<unsigned>(Mem(Addr(KaiserMoteB_Lift))[Cur()[7]]) << 6);
    SetWord(Cur() + 0xC, 0x40);
    Cur()[5] = 0x10;
    Cur()[6] = 8;
    Inc(Cur()[2]);
}

// original 0x4E6530: as KaiserMoteA_Spread, by KaiserMoteB_Angles.
S30_EXPORT void __cdecl KaiserMoteB_Spread(void) {
    AddB(Cur()[6], 2);
    AddW(Cur() + 0xC, 0x20);
    MotePlace(AngleB(), true);
    if (Word(Cur() + 0xC) == 0x200) Inc(Cur()[2]);
}

// original 0x4E65D0: as KaiserMoteA_Fade, by KaiserMoteB_Angles.
S30_EXPORT void __cdecl KaiserMoteB_Fade(void) {
    AddW(Cur() + 0xC, 0x10);
    MotePlace(AngleB(), true);
    AddB(Cur()[5], 0xFE);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::KaiserMote_Free);
}

// original 0x4E6670: a fan of eight gouraud triangles round the mote's screen
// point (+0x20, +0x22), radius +6 + Rand & 3; the centre's shade +5 x 14
// (kind 0) or +5 x (Rand & 3 + 4) (kind 1), the rim's 1.
S30_EXPORT void __cdecl KaiserMote_Draw(void) {
    std::uint32_t r = RandCall();
    unsigned char* c = Cur();
    SetSW(0, (r & 3) + c[6]);
    unsigned char shade;
    if (c[1] != 0) {
        r = RandCall();
        c = Cur();
        shade = static_cast<unsigned char>(((r & 3) + 4) * c[5]);
    } else {
        shade = static_cast<unsigned char>(c[5] * 14);
    }
    SetSW(0xC, Word(c + 0x20));
    SetSW(0xE, Word(c + 0x22));
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0xC));
        PutFloat(p + 0xC, SS(0xE));
        unsigned angle = (k << 9) & 0xFFF;
        SetSW(6, angle);
        int v = Sin(static_cast<short>(angle));
        PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(0xC));
        v = Cos(SS(6));
        PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xE));
        angle = ((k + 1) << 9) & 0xFFF;
        SetSW(6, angle);
        v = Sin(static_cast<short>(angle));
        PutFloat(p + 0x28, Mul12(v, SS(0)) + SS(0xC));
        v = Cos(SS(6));
        PutFloat(p + 0x2C, Mul12(v, SS(0)) + SS(0xE));
        p[4] = shade;
        p[5] = shade;
        p[6] = shade;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        Commit(2, 0x34);
    }
}

// original 0x4E6830: the first of the 48 motes whose +0 bit 0 is clear gets
// it set, and its index in al; 0xFF when none is free.
S30_EXPORT unsigned char __cdecl KaiserMote_Alloc(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) != 0) continue;
        m[0] = static_cast<unsigned char>(m[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4E6880: the current mote's +0..+4 cleared.
S30_EXPORT void __cdecl KaiserMote_Free(void) {
    Cur()[0] = 0;
    Cur()[1] = 0;
    Cur()[2] = 0;
    Cur()[3] = 0;
    Cur()[4] = 0;
}

// original 0x4E68B0: the mote's screen point, as BattleActor_UpdateScreenXY
// does an actor's: (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height's high word
// / 2)) of +0x14 / +0x18 / +0x1E projected into a TILE_1 at Gfx_PacketNext
// (never committed), the float x / y truncated to +0x20 / +0x22.
S30_EXPORT void __cdecl KaiserMote_UpdateScreenXY(void) {
    const unsigned char* const c = Cur();
    unsigned char* const p = Gfx_PacketNext;
    short v[4];
    v[0] = static_cast<short>((Long(c + 0x14) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(c + 0x18) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(c + 0x1E) / 2));
    v[3] = 0;
    MH_CALL(Gpu_SetTile1)(p);
    long depth, flag;
    S30_AS(Rtp1Fn, Gte_RotTransPers)(v, p + 8, &depth, &flag);
    MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
    SetWord(Cur() + 0x20, Ftol16(static_cast<std::uint32_t>(Long(p + 8))));
    SetWord(Cur() + 0x22, Ftol16(static_cast<std::uint32_t>(Long(p + 0xC))));
}

void MagicS30_Inject() {
    if (bof3::WantsShadow("magic_s30")) magic_s30::SelfTest();
    BOF3_INJECT(Venom_Task);
    BOF3_INJECT(Venom_Start);
    BOF3_INJECT(VenomChild_Task);
    BOF3_INJECT(VenomSprite_Run);
    BOF3_INJECT(VenomSprite_Start);
    BOF3_INJECT(VenomSprite_ShadeUp);
    BOF3_INJECT(VenomSprite_ShadeDown);
    BOF3_INJECT(VenomRing_Run);
    BOF3_INJECT(VenomRing_Start);
    BOF3_INJECT(VenomRing_Grow);
    BOF3_INJECT(MagicFx_CountDown9);
    BOF3_INJECT(VenomRing_DrawFan);
    BOF3_INJECT(VenomRing_DrawBand);
    BOF3_INJECT(Kaiser_Task);
    BOF3_INJECT(Kaiser_Start);
    BOF3_INJECT(Kaiser_LoadFormFile);
    BOF3_INJECT(Kaiser_HideParty);
    BOF3_INJECT(Kaiser_WaitChildren);
    BOF3_INJECT(Kaiser_ReloadParty);
    BOF3_INJECT(Kaiser_LoadSetFile);
    BOF3_INJECT(Kaiser_ShowParty);
    BOF3_INJECT(MagicFx_EndWhenChildrenDone);
    BOF3_INJECT(KaiserChild_Task);
    BOF3_INJECT(KaiserSprite_Run);
    BOF3_INJECT(KaiserSprite_Start);
    BOF3_INJECT(KaiserSprite_Glide);
    BOF3_INJECT(KaiserSprite_Brake);
    BOF3_INJECT(KaiserSprite_Tick);
    BOF3_INJECT(KaiserSprite_Breathe);
    BOF3_INJECT(KaiserSprite_WaitScript);
    BOF3_INJECT(KaiserSprite_Leave);
    BOF3_INJECT(KaiserSprite_Exit);
    BOF3_INJECT(KaiserFlash_Run);
    BOF3_INJECT(MagicFx_ClearCount9);
    BOF3_INJECT(MagicFx_CountUp9By2);
    BOF3_INJECT(KaiserFlash_WaitLoad);
    BOF3_INJECT(KaiserFlash_Draw);
    BOF3_INJECT(KaiserPillar_Run);
    BOF3_INJECT(KaiserPillar_Start);
    BOF3_INJECT(KaiserPillar_Grow);
    BOF3_INJECT(KaiserPillar_Fade);
    BOF3_INJECT(KaiserPillar_Draw);
    BOF3_INJECT(KaiserRing_Run);
    BOF3_INJECT(KaiserRing_Start);
    BOF3_INJECT(KaiserRing_Grow);
    BOF3_INJECT(KaiserRing_Fade);
    BOF3_INJECT(KaiserRing_Draw);
    BOF3_INJECT(KaiserMote_Task);
    BOF3_INJECT(KaiserMoteA_Run);
    BOF3_INJECT(KaiserMoteA_Start);
    BOF3_INJECT(KaiserMoteA_Spread);
    BOF3_INJECT(KaiserMoteA_Fade);
    BOF3_INJECT(KaiserMoteB_Run);
    BOF3_INJECT(KaiserMoteB_Start);
    BOF3_INJECT(KaiserMoteB_Spread);
    BOF3_INJECT(KaiserMoteB_Fade);
    BOF3_INJECT(KaiserMote_Draw);
    BOF3_INJECT(KaiserMote_Alloc);
    BOF3_INJECT(KaiserMote_Free);
    BOF3_INJECT(KaiserMote_UpdateScreenXY);
}
