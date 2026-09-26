// Round nine group S17: three BMAGIC overlays compiled into the exe, 48
// functions at 0x4BCBF0..0x4BEB45 (docs/magic_s17.md):
//
//   - MAGIC075, Magic_Rows row 46 (ids 0x4B / 0xB3, Purify read one id
//     down): a glow on the source sprite that brightens as seven motes land
//     round the target, then fades - 0x4BCBF0..0x4BCFA6;
//   - MAGIC077, row 30 (ids 0x4C / 0xB4 and 0x4D / 0xB5, Raise Dead and
//     Resurrect): a textured halo over the target that sheds motes - more,
//     and a larger halo, for any id but 0x4C / 0xB4 - from a pool of 64 at
//     0x68C0B8, run by the effect's own task - 0x4BCFB0..0x4BDAB6;
//   - MAGIC078, row 15 (ids 0x4E / 0xB6, Leech Power): a shell of quads over
//     the caster and a set of rings that rise from the target and merge into
//     it - 0x4BDAC0..0x4BEB45.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. Calls into functions other groups own - the
// effect library's 0x4FB880 / 0x4FB9F0 / 0x4FBBD0, MAGIC169's 0x4F1BD0,
// MAGIC219's 0x4F6290, and the phases MAGIC169's 0x4F1A40 and MAGIC060's
// 0x4B1740 - are raw addresses.
//
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through its own stack
// (docs/magic_fx_reached.md section 3, the precedent). The .data tables are
// read in place, their indexes unchecked, as the originals read them.
#include "game/magic_s17.h"

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

// --- the addresses ours reads and writes -----------------------------------

constexpr std::uint32_t kScratchA = 0x903850;       // DamageScratch (the PSX scratchpad): radius, angle words
constexpr std::uint32_t kScratchB = 0x903854;
constexpr std::uint32_t kShade = 0x903856;          // u16: a shade the draws copy to their primitives
constexpr std::uint32_t kRgb = 0x903858;            // three u16 at +0 / +2 / +4
constexpr std::uint32_t kVertex = 0x9037A0;         // Prim_VertexScratch: four SVECTORs, 8 bytes apart
constexpr std::uint32_t kFrameSet = 0x9039D8;       // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kActingKind = 0x904B35;     // u8: 4 an ability
constexpr std::uint32_t kAbility = 0x904B80;        // u16: the ability id
constexpr std::uint32_t kTints = 0x7E0700;          // MoveScript_TintRecords: 12 bytes each, +2..+4 r g b
constexpr std::uint32_t kClutRow26 = 0x812980;      // Gfx_ClutStrip row 26 (VRAM row 506)
constexpr std::uint32_t kClutRow26Source = 0x80E980;
constexpr std::uint32_t kClutRow2 = 0x80F980;       // Gfx_ClutStrip row 2 (VRAM row 482)
constexpr std::uint32_t kClutRow2Source = 0x80B980;

// MAGIC077's mote pool: 64 of 0x84 bytes, +0 bit 0 in use.
constexpr std::uint32_t kMotePool = 0x68C0B8;
constexpr std::uint32_t kMoteStride = 0x84;
constexpr unsigned kMotes = 0x40;

// The .data tables, read in place.
constexpr std::uint32_t kPurifyMoteTypes = 0x65B1B4;    // 1 entry by +1
constexpr std::uint32_t kPurifyMotePhases = 0x65B1B8;   // 2 entries by +2
constexpr std::uint32_t kHaloTypes = 0x65B1C0;          // 1 entry by +1
constexpr std::uint32_t kHaloPhases = 0x65B1C4;         // 4 entries by +2
constexpr std::uint32_t kHaloMoteCounts = 0x65B1D4;     // u8 by the halo's +4
constexpr std::uint32_t kMoteTypes = 0x65B1D8;          // 1 entry by +1
constexpr std::uint32_t kLeechOrbTypes = 0x65B1DC;      // 2 entries by +1
constexpr std::uint32_t kShellPhases = 0x65B1E4;        // 7 entries by +2
constexpr std::uint32_t kShellAngles = 0x65B200;        // eleven dwords: the shell's longitudes, then latitudes
constexpr std::uint32_t kOrbPhases = 0x65B228;          // 6 entries by +2

// Callees other groups own, by address.
constexpr std::uint32_t kSortPrims = 0x4FB880;     // LIBRARY: (x, z, long *depths, prims, count, stride, dy)
constexpr std::uint32_t kSeek = 0x4FB9F0;          // LIBRARY: (target, s16 speed) Sprite_Current toward target
constexpr std::uint32_t kNear = 0x4FBBD0;          // LIBRARY: (target, range) -> 1 within range
constexpr std::uint32_t kMoteGlow = 0x4F1BD0;      // MAGIC169: a mote's second draw
constexpr std::uint32_t kMoteDone = 0x4F6290;      // MAGIC219: 35 files' end of a child
using Void = void (__cdecl*)();
using SortFn = void (__cdecl*)(std::uint32_t, std::uint32_t, long*, unsigned char*, unsigned, unsigned, unsigned);
using SeekFn = void (__cdecl*)(unsigned char*, int);
using NearFn = int (__cdecl*)(unsigned char*, unsigned);

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
unsigned char* Task(unsigned slot) { return Mem(at::kTasks + (slot & 0xFF) * at::kTaskStride); }
unsigned char* Mote(unsigned i) { return Mem(kMotePool + (i & 0xFF) * kMoteStride); }
unsigned char* Tint(unsigned i) { return Mem(kTints + (i & 0xFF) * 12u); }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::int32_t Table(std::uint32_t at) { return Long(Mem(at)); }
short* V(unsigned k) { return reinterpret_cast<short*>(Mem(kVertex + k)); }
unsigned char* VB(unsigned k) { return Mem(kVertex + k); }
float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int M(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
int Wrap(std::uint32_t v) { return static_cast<int>(v); }

// `fild dword` then `fst(p) dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }
std::int32_t R() { return Long(Mem(kScratchA)); }
std::int32_t H() { return Long(Mem(kScratchB)); }

// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0) and its commit.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
void LinkAtActor(int dy, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)), dy,
                                size);
}
void ModeCommit4() {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(4, 0xC);
}

// The target's done flag, 0x904AA8 bit 2, and the task freed: every end of
// an effect here.
void TargetDone() {
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// A mote's position round the owner: +0x34 / +0x38 = the owner's plus
// sin / cos of the angle (word 0x903854) times the radius +0xC, each 32-bit.
void Orbit() {
    const int s = Sin(S16(Mem(kScratchB)));
    unsigned char* sc = Sprite_Current;
    SetLong(sc + 0x34, Wrap(static_cast<std::uint32_t>(s) * static_cast<std::uint32_t>(Long(sc + 0xC)) +
                            static_cast<std::uint32_t>(Long(Owner() + 0x34))));
    const int c = Cos(S16(Mem(kScratchB)));
    sc = Sprite_Current;
    SetLong(sc + 0x38, Wrap(static_cast<std::uint32_t>(c) * static_cast<std::uint32_t>(Long(sc + 0xC)) +
                            static_cast<std::uint32_t>(Long(Owner() + 0x38))));
}
void MoteAngle() { SetWord(Mem(kScratchB), static_cast<unsigned>((Sprite_Current[0xB] & 0xF) << 8)); }

}  // namespace

#define MS17_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC075 (row 46): the glow and its seven motes

// original 0x4BCBF0: the kind-2 task. Its phase +1 through a three-entry
// table on its stack - Purify_Start, Purify_Glow, Purify_Fade. 3..255 would
// call through the original's stack; ours aborts.
MS17_EXPORT void __cdecl Purify_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Purify_Start, bof3::addr::Purify_Glow, bof3::addr::Purify_Fade};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 3) bof3::Fatal("Purify_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4BCC20: the source sprite's (0x904B4C, read once) position to
// the task; +0xB 0; on. Seven motes (kind 1, parameter 0x25), each with this
// task its owner, +1 0, +0xB its number 0..6 and +9 its delay 1, 6, .. 31.
// Then the source's tint released and a new one (0, 0, 0, 1) to +0xA; CLUT
// row 26's first 16 cells from their source 0x4000 below; the strip dirty;
// sound 0x100.
MS17_EXPORT void __cdecl Purify_Start(void) {
    unsigned char* const src = Source();
    SetLong(Sprite_Current + 0x34, Long(src + 0x34));
    SetLong(Sprite_Current + 0x38, Long(src + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(src + 0x3C));
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
    unsigned char number = 0;
    for (unsigned char delay = 1; delay < 0x24; delay = static_cast<unsigned char>(delay + 5)) {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x25) & 0xFF;
        unsigned char* const t = Task(slot);
        SetLong(t + 0x80, static_cast<std::int32_t>(Key(Sprite_Current)));
        t[1] = 0;
        t[0xB] = number;
        t[9] = delay;
        ++number;
    }
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    for (unsigned i = 0; i < 0x10; ++i) SetWord(Mem(kClutRow26 + 2 * i), Word(Mem(kClutRow26Source + 2 * i)));
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4BCD10: the tint record +0xA's r, g, b each +0xB * 2 (the motes
// landed so far); at seven, on.
MS17_EXPORT void __cdecl Purify_Glow(void) {
    unsigned char* const sc = Sprite_Current;
    Tint(sc[0xA])[2] = static_cast<unsigned char>(sc[0xB] << 1);
    Tint(sc[0xA])[3] = static_cast<unsigned char>(sc[0xB] << 1);
    Tint(sc[0xA])[4] = static_cast<unsigned char>(sc[0xB] << 1);
    if (sc[0xB] == 7) Bump(sc[1]);
}

// original 0x4BCD60: the tint's r, g, b each down 2; once r is 0 the tint
// released, the target's done flag, the task freed (a tail jmp).
MS17_EXPORT void __cdecl Purify_Fade(void) {
    unsigned char* const sc = Sprite_Current;
    Bump(Tint(sc[0xA])[2], -2);
    Bump(Tint(sc[0xA])[3], -2);
    Bump(Tint(sc[0xA])[4], -2);
    const unsigned char index = sc[0xA];
    if (Tint(index)[2] != 0) return;
    MH_CALL(Tint_Release)(index);
    TargetDone();
}

// original 0x4BCDF0: the mote (kind 1, parameter 0x25): a jmp through
// PurifyMote_Types 0x65B1B4 by +1, read in place (one entry).
MS17_EXPORT void __cdecl PurifyMote_Dispatch(void) { Entry(kPurifyMoteTypes, Sprite_Current[1])(); }

// original 0x4BCE10: with the frame-offset table 0x9039D8 the effects'
// (0x8E3580), a call through PurifyMote_Phases 0x65B1B8 by +2, read in place
// (two entries); then while it lives and is placed (+0, +2 not 0) its
// sprite's screen update; the battle's table (0x8B3580) back.
MS17_EXPORT void __cdecl PurifyMote_Task(void) {
    SetLong(Mem(kFrameSet), kFrameSetEffect);
    Entry(kPurifyMotePhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), kFrameSetBattle);
}

// original 0x4BCE50: +9 down; at 0 the mote placed round its owner - angle
// (+0xB & 7) << 9 (to 0x903850), +0x34 / +0x38 the owner's plus sin / cos
// times 10 (as 5 << 13 >> 12, 32-bit), height the owner's +0x3E + 0x180 -
// its sprite fields set (+0x24 4, +0x25 0x1D, +0x27 0xA0, +0x29 4, +0x2B 1,
// the rest 0), Sprite_SetAnimation(+0xB / 3), and +2 on.
MS17_EXPORT void __cdecl PurifyMote_Place(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* sc = Sprite_Current;
    if (sc[9] != 0) return;
    const int angle = (sc[0xB] & 7) << 9;
    SetLong(Mem(kScratchA), angle);
    const int s = Sin(angle);
    const int dx = static_cast<int>((static_cast<std::uint32_t>(s) * 5u) << 13) >> 12;
    SetLong(Sprite_Current + 0x34, Wrap(static_cast<std::uint32_t>(dx) + static_cast<std::uint32_t>(Long(Owner() + 0x34))));
    const int c = Cos(Long(Mem(kScratchA)));
    const int dz = static_cast<int>((static_cast<std::uint32_t>(c) * 5u) << 13) >> 12;
    SetLong(Sprite_Current + 0x38, Wrap(static_cast<std::uint32_t>(dz) + static_cast<std::uint32_t>(Long(Owner() + 0x38))));
    SetWord(Sprite_Current + 0x3E, Word(Owner() + 0x3E) + 0x180u);
    sc = Sprite_Current;
    sc[0x25] = 0x1D;
    sc[0x26] = 0;
    sc[0x27] = 0xA0;
    sc[0x28] = 0;
    sc[0x5D] = 0;
    sc[0x5E] = 0;
    sc[0x5F] = 0;
    sc[0x5C] = 0;
    sc[0x2A] = 0;
    sc[0x29] = 4;
    sc[0x24] = 4;
    SetWord(sc + 0x2C, 0);
    sc[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(sc[0xB] / 3));
    Bump(Sprite_Current[2]);
}

// original 0x4BCF90: the mote's script once; at its end the owner's count
// +0xB up and the mote freed (a tail jmp).
MS17_EXPORT void __cdecl PurifyMote_End(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Bump(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// MAGIC077 (row 30): the halo and its motes

// original 0x4BCFB0: the kind-2 task. Its phase +1 through a six-entry table
// on its stack - Revive_Start, Revive_TintSource, BattleFx_Brighten (round
// eight's), Revive_WaitMotes, Revive_Fade, BattleFx_Finish (round eight's);
// 6..255 would call through the original's stack, ours aborts. Then every
// live mote of the pool 0x68C0B8 run: Sprite_Current the mote and 0x93B940
// its +0x80 for the call, both put back after (as they were after the
// phase).
MS17_EXPORT void __cdecl Revive_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Revive_Start,     bof3::addr::Revive_TintSource,
                                                 bof3::addr::BattleFx_Brighten, bof3::addr::Revive_WaitMotes,
                                                 bof3::addr::Revive_Fade,      bof3::addr::BattleFx_Finish};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 6) bof3::Fatal("Revive_Task: phase %u, past the six-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const saved = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = Long(m + 0x80);
        Sprite_Current = m;
        SetLong(Mem(at::kOwner), its);
        MH_CALL(ReviveMote_Dispatch)();
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = saved;
    }
}

// original 0x4BD050: the pool emptied (+0..+2 of all 64); +4 the strength
// (Revive_IsStrong); the source's actor byte and position to the task; +0xB
// 0, +9 0x3C, +0xA 0; on. The halo (kind 1, parameter 0x62: this task its
// owner, +1 0, +9 1), the task's count +0xB up; sound 0x100. CLUT row 26
// whole and row 2's first 16 cells from their sources with the
// semi-transparency bit, each row's cell 0 without; the strip dirty.
MS17_EXPORT void __cdecl Revive_Start(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    const unsigned char strong = MH_CALL(Revive_IsStrong)();
    Sprite_Current[4] = strong;
    unsigned char* const src = Source();
    Sprite_Current[8] = src[8];
    SetLong(Sprite_Current + 0x34, Long(src + 0x34));
    SetLong(Sprite_Current + 0x38, Long(src + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(src + 0x3C));
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0x3C;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x62) & 0xFF;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const t = Task(slot);
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(sc)));
    t[1] = 0;
    t[9] = 1;
    Bump(sc[0xB]);
    MH_CALL(Sound_PlayById)(0x100);
    for (unsigned i = 0; i < 0x100; ++i) SetWord(Mem(kClutRow26 + 2 * i), Word(Mem(kClutRow26Source + 2 * i)) | 0x8000u);
    for (unsigned i = 0; i < 0x10; ++i) SetWord(Mem(kClutRow2 + 2 * i), Word(Mem(kClutRow2Source + 2 * i)) | 0x8000u);
    SetWord(Mem(kClutRow26), Word(Mem(kClutRow26Source)));
    SetWord(Mem(kClutRow2), Word(Mem(kClutRow2Source)));
    Gfx_ClutStripDirty = 1;
}

// original 0x4BD180: +9 down; at 0 the source's tint released and a new one
// (0, 0, 0, 1) to +0xA, the source read again for each; +9 8; on.
MS17_EXPORT void __cdecl Revive_TintSource(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 8;
    Bump(Sprite_Current[1]);
}

// original 0x4BD1E0: held until the count +0xB is 0 (the halo and its motes
// gone); then +9 8, on.
MS17_EXPORT void __cdecl Revive_WaitMotes(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[0xB] != 0) return;
    sc[9] = 8;
    Bump(sc[1]);
}

// original 0x4BD200: the tint's r, g, b each down 1; +9 down; at 0 the
// source's tint released, on.
MS17_EXPORT void __cdecl Revive_Fade(void) {
    unsigned char* const sc = Sprite_Current;
    Bump(Tint(sc[0xA])[2], -1);
    Bump(Tint(sc[0xA])[3], -1);
    Bump(Tint(sc[0xA])[4], -1);
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    Bump(Sprite_Current[1]);
}

// original 0x4BD280: 1 when the acting kind 0x904B35 is 4 (an ability) and
// the ability 0x904B80 (u16) is neither 0x4C nor 0xB4; else 0. In al.
MS17_EXPORT unsigned char __cdecl Revive_IsStrong(void) {
    if (Mem(kActingKind)[0] != 4) return 0;
    const unsigned id = Word(Mem(kAbility));
    return id == 0x4C || id == 0xB4 ? 0 : 1;
}

// original 0x4BD2B0: the halo (kind 1, parameter 0x62): a jmp through
// ReviveHalo_Types 0x65B1C0 by +1, read in place (one entry).
MS17_EXPORT void __cdecl ReviveHalo_Dispatch(void) { Entry(kHaloTypes, Sprite_Current[1])(); }

// original 0x4BD2D0: a call through ReviveHalo_Phases 0x65B1C4 by +2, read in
// place (four entries: ReviveHalo_Wait, ReviveHalo_Spawn, MagicFx_WaitA,
// MAGIC060's 0x4B1740); then while it lives and has started (+0, +2 not 0)
// its screen point and its draw (a tail jmp).
MS17_EXPORT void __cdecl ReviveHalo_Task(void) {
    Entry(kHaloPhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(ReviveHalo_Draw)();
}

// original 0x4BD300: +9 down; at 0 the owner's strength +4 and position to
// the halo, 0xC00000 higher; +9 0, +0xA 0x3C; +2 on.
MS17_EXPORT void __cdecl ReviveHalo_Wait(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    sc[4] = Owner()[4];
    SetLong(Sprite_Current + 0x34, Long(Owner() + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Owner() + 0x38));
    SetLong(Sprite_Current + 0x3C, Wrap(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) + 0xC00000u));
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0x3C;
    Bump(Sprite_Current[2]);
}

// original 0x4BD380: +9 up; at 0x10 the motes - as many as
// ReviveHalo_MoteCounts 0x65B1D4 [+4] (u8, read in place, the index
// unchecked; rows 0 and 1 are the weak and the strong), each from the pool (ReviveMote_Alloc; none left, none):
// the owner its +0x80, +1 0, +0xB its number, +9 16..31 by Rand, the owner's
// count +0xB up - and +2 on.
MS17_EXPORT void __cdecl ReviveHalo_Spawn(void) {
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x10) return;
    unsigned char number = 0;
    if (Mem(kHaloMoteCounts + Sprite_Current[4])[0] != 0) {
        do {
            const unsigned char index = MH_CALL(ReviveMote_Alloc)();
            if (index != 0xFF) {
                unsigned char* const m = Mote(index);
                SetLong(m + 0x80, Long(Mem(at::kOwner)));
                m[1] = 0;
                m[0xB] = number;
                const int r = MH_CALL(Rand)();
                m[9] = static_cast<unsigned char>((r & 0xF) + 0x10);
                Bump(Owner()[0xB]);
            }
            ++number;
        } while (number < Mem(kHaloMoteCounts + Sprite_Current[4])[0]);
    }
    Bump(Sprite_Current[2]);
}

// original 0x4BD420: the halo: a draw mode (tpage 0xB5) sorted at the halo,
// then a semi-transparent textured quad over its screen point (+0x2E /
// +0x30), from y - 0x60 (strong) or y - 0x40 to y, half-width 0x38 or 0x18;
// tpage (1, 1, 0x340, 0x100), clut (0, 0x1FA), texture cells by the
// strength; the shade +9 * 8 (word 0x903856); sorted at the halo, size 0x48.
MS17_EXPORT void __cdecl ReviveHalo_Draw(void) {
    DrawMode(0xB5);
    LinkAtActor(1, 0xC);
    const unsigned char* sc = Sprite_Current;
    int height, half;
    if (sc[4] != 0) {
        height = 0x60;
        half = 0x38;
    } else {
        height = 0x40;
        half = 0x18;
    }
    SetWord(Mem(kShade), static_cast<unsigned>(sc[9]) << 3);
    unsigned char* const p = Gfx_PacketNext;
    const int x = S16(sc + 0x2E);
    const int y = S16(sc + 0x30);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, x - half);
    PutFloat(p + 0xC, y - height);
    PutFloat(p + 0x18, x + half);
    PutFloat(p + 0x1C, y - height);
    PutFloat(p + 0x38, x + half);
    PutFloat(p + 0x28, x - half);
    PutFloat(p + 0x2C, y);
    PutFloat(p + 0x3C, y);
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    if (Sprite_Current[4] != 0) {
        p[0x14] = 8;
        p[0x15] = 8;
        p[0x25] = 8;
        p[0x34] = 8;
        p[0x24] = 0x78;
        p[0x35] = 0x68;
        p[0x44] = 0x78;
        p[0x45] = 0x68;
    } else {
        p[0x14] = 0x78;
        p[0x15] = 8;
        p[0x24] = 0xA8;
        p[0x25] = 8;
        p[0x34] = 0x78;
        p[0x35] = 0x48;
        p[0x44] = 0xA8;
        p[0x45] = 0x48;
    }
    p[4] = Mem(kShade)[0];
    p[5] = Mem(kShade)[0];
    p[6] = Mem(kShade)[0];
    LinkAtActor(1, 0x48);
}

// original 0x4BD5C0: a mote: a jmp through ReviveMote_Types 0x65B1D8 by +1,
// read in place (one entry).
MS17_EXPORT void __cdecl ReviveMote_Dispatch(void) { Entry(kMoteTypes, Sprite_Current[1])(); }

// original 0x4BD5E0: its phase +2 through a three-entry table on its stack -
// ReviveMote_Launch, MAGIC169's 0x4F1A40, ReviveMote_Rise; 3..255 would call
// through the original's stack, ours aborts. Then between two draw modes
// (tpage 0x35, then 0x15, each sorted at the mote) while it lives and has
// launched (+0, +2 not 0): its screen point, its fan, and MAGIC169's
// 0x4F1BD0.
MS17_EXPORT void __cdecl ReviveMote_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::ReviveMote_Launch, 0x4F1A40, bof3::addr::ReviveMote_Rise};
    const unsigned phase = Sprite_Current[2];
    if (phase >= 3) bof3::Fatal("ReviveMote_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(0x35);
    LinkAtActor(2, 0xC);
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] != 0 && sc[2] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(ReviveMote_Draw)();
        MH_AT(Void, kMoteGlow)();
    }
    DrawMode(0x15);
    LinkAtActor(2, 0xC);
}

// original 0x4BD690: +9 down; at 0 (the first mote with sound 0x101) the
// mote launched from the owner: radius +0xC 8, angle (+0xB & 0xF) << 8 (word
// 0x903854) and its orbit; height the owner's + 0x800000; rise +0x14
// ((Rand & 3) + 2) << 20 and its step +0x20 a sixteenth of it; colour
// +0x5D..+0x5F each (Rand & 7) + 6; +9 0, +0xA 0x10; +2 on.
MS17_EXPORT void __cdecl ReviveMote_Launch(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    if (Sprite_Current[0xB] == 0) MH_CALL(Sound_PlayById)(0x101);
    SetLong(Sprite_Current + 0xC, 8);
    MoteAngle();
    Orbit();
    SetLong(Sprite_Current + 0x3C, Wrap(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) + 0x800000u));
    const int r = MH_CALL(Rand)();
    SetLong(Sprite_Current + 0x14, ((r & 3) + 2) << 20);
    unsigned char* sc = Sprite_Current;
    SetLong(sc + 0x20, Long(sc + 0x14) / 16);
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const int v = MH_CALL(Rand)();
        Sprite_Current[k] = static_cast<unsigned char>((v & 7) + 6);
    }
    sc = Sprite_Current;
    sc[9] = 0;
    sc[0xA] = 0x10;
    Bump(sc[2]);
}

// original 0x4BD7D0: the radius +0xC up one and the orbit again; while the
// step +0x20 is below the rise +0x14 (signed), the rise less the step, and
// the height up by it; on odd frames +9 down, and at 0 the owner's count
// +0xB down and MAGIC219's 0x4F6290 (a tail jmp).
MS17_EXPORT void __cdecl ReviveMote_Rise(void) {
    SetLong(Sprite_Current + 0xC, Wrap(static_cast<std::uint32_t>(Long(Sprite_Current + 0xC)) + 1u));
    MoteAngle();
    Orbit();
    unsigned char* sc = Sprite_Current;
    const std::int32_t step = Long(sc + 0x20), rise = Long(sc + 0x14);
    if (step < rise) {
        SetLong(sc + 0x14, Wrap(static_cast<std::uint32_t>(rise) - static_cast<std::uint32_t>(step)));
        sc = Sprite_Current;
        SetLong(sc + 0x3C, Wrap(static_cast<std::uint32_t>(Long(sc + 0x3C)) + static_cast<std::uint32_t>(Long(sc + 0x14))));
        sc = Sprite_Current;
    }
    if ((Frame_Counter & 1) == 0) return;
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_AT(Void, kMoteDone)();
}

// original 0x4BD8A0: a fan of eight semi-transparent gouraud triangles round
// the mote's screen point (+0x2E / +0x30), radius 8 (word 0x903850, read
// back each time), the centre shade +9 * 12 (word 0x903856), the rim +9
// times the colour +0x5D..+0x5F (signed, words 0x903858..), each sorted at
// the mote, size 0x34.
MS17_EXPORT void __cdecl ReviveMote_Draw(void) {
    const unsigned char* const sc = Sprite_Current;
    const int x = S16(sc + 0x2E);
    const int y = S16(sc + 0x30);
    SetWord(Mem(kScratchA), 8);
    SetWord(Mem(kShade), sc[9] * 12u);
    SetWord(Mem(kRgb), static_cast<unsigned>(static_cast<signed char>(sc[0x5D]) * sc[9]));
    SetWord(Mem(kRgb + 2), static_cast<unsigned>(static_cast<signed char>(sc[0x5E]) * sc[9]));
    SetWord(Mem(kRgb + 4), static_cast<unsigned>(static_cast<signed char>(sc[0x5F]) * sc[9]));
    unsigned char centre[8];
    PutFloat(centre, x);
    PutFloat(centre + 4, y);
    int a = 0;
    for (int n = 8; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::memcpy(p + 8, centre, 8);
        int v = Sin(a);
        PutFloat(p + 0x18, M(v, S16(Mem(kScratchA))) + x);
        v = Cos(a);
        PutFloat(p + 0x1C, M(v, S16(Mem(kScratchA))) + y);
        a += 0x200;
        v = Sin(a);
        PutFloat(p + 0x28, M(v, S16(Mem(kScratchA))) + x);
        v = Cos(a);
        PutFloat(p + 0x2C, M(v, S16(Mem(kScratchA))) + y);
        p[4] = Mem(kShade)[0];
        p[5] = Mem(kShade)[0];
        p[6] = Mem(kShade)[0];
        p[0x14] = Mem(kRgb)[0];
        p[0x15] = Mem(kRgb + 2)[0];
        p[0x16] = Mem(kRgb + 4)[0];
        p[0x24] = Mem(kRgb)[0];
        p[0x25] = Mem(kRgb + 2)[0];
        p[0x26] = Mem(kRgb + 4)[0];
        LinkAtActor(2, 0x34);
    }
}

// original 0x4BDA60: the first free mote of the pool (+0 bit 0 clear) marked
// in use; its index in al, or 0xFF when all 64 are.
MS17_EXPORT unsigned char __cdecl ReviveMote_Alloc(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) == 0) {
            m[0] = static_cast<unsigned char>(m[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC078 (row 15): the shell and the orb

// original 0x4BDAC0: the kind-2 task. Between two draw modes (tpage 0x35,
// committed to slot 4) its phase +1 through a two-entry table on its stack -
// Leech_Start, Leech_WaitOrbs; 2..255 would call through the original's
// stack, ours aborts.
MS17_EXPORT void __cdecl Leech_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Leech_Start, bof3::addr::Leech_WaitOrbs};
    ModeCommit4();
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Leech_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    ModeCommit4();
}

// original 0x4BDB20: two children (kind 1, parameter 0xD): the shell (+1 0,
// this task its owner, at the owner 0x2000000 higher) and the orb (+1 1, the
// shell its owner and this task its +0x4C, at the source 0x904B4C - read
// before either - 0x1000000 higher), each +9 0; sound 0x100; +0xB 0; on.
MS17_EXPORT void __cdecl Leech_Start(void) {
    unsigned char* const src = Source();
    const unsigned s1 = MH_CALL(BattleTask_Create)(1, 0xD) & 0xFF;
    unsigned char* const shell = Task(s1);
    SetLong(shell + 0x80, static_cast<std::int32_t>(Key(Sprite_Current)));
    unsigned char* const owner = Owner();
    shell[1] = 0;
    shell[9] = 0;
    SetLong(shell + 0x34, Long(owner + 0x34));
    SetLong(shell + 0x38, Long(owner + 0x38));
    SetLong(shell + 0x3C, Wrap(static_cast<std::uint32_t>(Long(owner + 0x3C)) + 0x2000000u));
    const unsigned s2 = MH_CALL(BattleTask_Create)(1, 0xD) & 0xFF;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const orb = Task(s2);
    SetLong(orb + 0x80, static_cast<std::int32_t>(Key(shell)));
    SetLong(orb + 0x4C, static_cast<std::int32_t>(Key(sc)));
    orb[1] = 1;
    orb[9] = 0;
    SetLong(orb + 0x34, Long(src + 0x34));
    SetLong(orb + 0x38, Long(src + 0x38));
    SetLong(orb + 0x3C, Wrap(static_cast<std::uint32_t>(Long(src + 0x3C)) + 0x1000000u));
    MH_CALL(Sound_PlayById)(0x100);
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4BDC10: held until +0xB is 0xFF (the shell's end); then the
// target's done flag and the task freed (a tail jmp).
MS17_EXPORT void __cdecl Leech_WaitOrbs(void) {
    if (Sprite_Current[0xB] != 0xFF) return;
    TargetDone();
}

// original 0x4BDC40: a child (kind 1, parameter 0xD): a jmp through
// LeechOrb_Types 0x65B1DC by +1, read in place (LeechShell_Task,
// LeechOrb_Task).
MS17_EXPORT void __cdecl LeechOrb_Dispatch(void) { Entry(kLeechOrbTypes, Sprite_Current[1])(); }

// original 0x4BDC60: the shell. Between two draw modes (tpage 0x35, slot 4):
// before phase 3 its spin +0xC from the frame counter ((low byte & 0x3F) <<
// 6, or 0x1000 at 0x40); a call through LeechShell_Phases 0x65B1E4 by +2,
// read in place (seven entries); while the owner's +0xB is not 0xFF, the
// shell drawn under its own matrix.
MS17_EXPORT void __cdecl LeechShell_Task(void) {
    ModeCommit4();
    unsigned char* sc = Sprite_Current;
    if (sc[2] < 3) {
        const unsigned frame = Frame_Counter;
        SetLong(sc + 0xC, (frame & 0xFF) == 0x40 ? 0x1000 : static_cast<std::int32_t>((frame & 0x3F) << 6));
        sc = Sprite_Current;
    }
    Entry(kShellPhases, sc[2])();
    if (Owner()[0xB] != 0xFF) {
        MH_CALL(LeechShell_PushMatrix)();
        MH_CALL(LeechShell_Draw)();
        MH_CALL(Gte_PopMatrix)();
    }
    ModeCommit4();
}

// original 0x4BDD00: +9 up to 12; there +0xA 15 and +2 on.
MS17_EXPORT void __cdecl LeechShell_Grow(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0xC) {
        sc[0xA] = 0xF;
        Bump(Sprite_Current[2]);
        return;
    }
    Bump(sc[9]);
}

// original 0x4BDD20 (reached from 17 files): +0xA down; at 0, +2 on.
MS17_EXPORT void __cdecl MagicFx_WaitA(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] == 0) Bump(Sprite_Current[2]);
}

// original 0x4BDD40: held until the spin faces the source: Math_Ratan2(dz,
// dx) of the source's position less the shell's (each as a float), >> 6 &
// 0x3F, equal to +0xC / 64 (signed); then +0xA 15, +2 on.
MS17_EXPORT void __cdecl LeechShell_Aim(void) {
    const unsigned char* const src = Source();
    const unsigned char* sc = Sprite_Current;
    const int dx = Wrap(static_cast<std::uint32_t>(Long(src + 0x34)) - static_cast<std::uint32_t>(Long(sc + 0x34)));
    const int dz = Wrap(static_cast<std::uint32_t>(Long(src + 0x38)) - static_cast<std::uint32_t>(Long(sc + 0x38)));
    const int r = MH_CALL(Math_Ratan2)(static_cast<float>(dz), static_cast<float>(dx));
    unsigned char* const now = Sprite_Current;
    if (Long(now + 0xC) / 64 != ((r >> 6) & 0x3F)) return;
    now[0xA] = 0xF;
    Bump(Sprite_Current[2]);
}

// original 0x4BDDB0: +0xA down; at 0 the owner's +0xB 1 (the orb may go),
// +2 on.
MS17_EXPORT void __cdecl LeechShell_Hold(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    Owner()[0xB] = 1;
    Bump(Sprite_Current[2]);
}

// original 0x4BDDE0: held until the owner's +0xB is 2 (the orb merged); then
// +0xA 0, +2 on.
MS17_EXPORT void __cdecl LeechShell_WaitOrb(void) {
    if (Owner()[0xB] != 2) return;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4BDE00: +0xA up 8; past 0x80 (unsigned), +2 on.
MS17_EXPORT void __cdecl LeechShell_Fade(void) {
    Bump(Sprite_Current[0xA], 8);
    if (Sprite_Current[0xA] > 0x80) Bump(Sprite_Current[2]);
}

// original 0x4BDE20: +9 down; at 0 the owner's +0xB 0xFF and the shell freed
// (a tail jmp).
MS17_EXPORT void __cdecl LeechShell_End(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// The shell's vertex for latitude `lat`, longitude `lon`: height cos(lat) *
// (+9 * 7) (dword 0x903854), and sin(lat) * (+9 * 10) (dword 0x903850) round
// by cos / sin(lon) - the calls in the original's order: cos(lat), then
// sin(lat) and cos(lon), then sin(lat) and sin(lon).
void ShellVertex(unsigned k, int lat, int lon) {
    SetWord(VB(k), static_cast<unsigned>(M(Cos(lat), H())));
    int s = Sin(lat);
    int t = M(s, R());
    int c = Cos(lon);
    SetWord(VB(k + 2), static_cast<unsigned>(M(t, c)));
    s = Sin(lat);
    t = M(s, R());
    c = Sin(lon);
    SetWord(VB(k + 4), static_cast<unsigned>(M(t, c)));
}
void CopyVertex(unsigned to, unsigned from) {
    const unsigned x = Word(VB(from)), y = Word(VB(from + 2)), z = Word(VB(from + 4));
    SetWord(VB(to), x);
    SetWord(VB(to + 2), y);
    SetWord(VB(to + 4), z);
}

}  // namespace

// original 0x4BDE50: the shell. A draw mode (tpage 0xB5) sorted at the
// shell; radius +9 * 10 and height +9 * 7 (dwords 0x903850 / 0x903854).
// Two bands of six semi-transparent quads (latitudes from
// LeechShell_Angles +0x1C..+0x24, longitudes from its first six dwords, the
// first edge at 0x180), each outlined by a line quad (LeechShell_Edge) written 0x380 past the
// first quad; blue (0x10, 0x10, 0x80), the one quad of the band whose
// number is frame >> 2 & 7 lit in phase 3, fading by +0xA from phase 5. Then
// two caps of two quads (latitudes +0x1C and +0x24) the same way (1, 1,
// 0x80). The 16 quads, then the 12 lines, sorted by their depths (0x4FB880),
// and Gfx_PacketNext left past the lines.
MS17_EXPORT void __cdecl LeechShell_Draw(void) {
    DrawMode(0xB5);
    LinkAtActor(2, 0xC);
    const unsigned char* sc = Sprite_Current;
    SetLong(Mem(kScratchA), sc[9] * 10);
    SetLong(Mem(kScratchB), sc[9] * 7);
    unsigned char* const first = Gfx_PacketNext;
    unsigned char* const line_base = first + 0x380;
    unsigned char* lines = line_base;
    long depths[16];
    long line_depths[12];
    long projected;
    unsigned d = 0;
    for (std::uint32_t lat = kShellAngles + 0x20; lat < kShellAngles + 0x28; lat += 4) {
        // the band's first edge, at longitude 0x180
        SetWord(VB(8), static_cast<unsigned>(M(Cos(Table(lat - 4)), H())));
        int s = Sin(Table(lat - 4));
        int t = M(s, R());
        int c = Cos(0x180);
        SetWord(VB(0xA), static_cast<unsigned>(M(t, c)));
        s = Sin(Table(lat - 4));
        t = M(s, R());
        c = Sin(0x180);
        SetWord(VB(0xC), static_cast<unsigned>(M(t, c)));
        SetWord(VB(0x18), static_cast<unsigned>(M(Cos(Table(lat)), H())));
        s = Sin(Table(lat));
        t = M(s, R());
        c = Cos(0x180);
        SetWord(VB(0x1A), static_cast<unsigned>(M(t, c)));
        s = Sin(Table(lat));
        t = M(s, R());
        c = Sin(0x180);
        SetWord(VB(0x1C), static_cast<unsigned>(M(t, c)));
        unsigned number = 1;
        for (std::uint32_t lon = kShellAngles + 4; lon < kShellAngles + 0x1C; lon += 4, ++number, ++d) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            CopyVertex(0, 8);
            ShellVertex(8, Table(lat - 4), Table(lon));
            CopyVertex(0x10, 0x18);
            ShellVertex(0x18, Table(lat), Table(lon));
            MH_CALL(LeechShell_Edge)(lines, &line_depths[d], Long(VB(0)), Long(VB(4)), Long(VB(8)), Long(VB(0xC)),
                                     Long(VB(0x18)), Long(VB(0x1C)), Long(VB(0x10)), Long(VB(0x14)));
            lines += 0x38;
            depths[d] = MH_CALL(Gte_RotAverage4)(V(0), V(8), V(0x10), V(0x18), F(p + 8), F(p + 0x14), F(p + 0x20),
                                                 F(p + 0x2C), &projected);
            p[4] = 0x10;
            p[5] = 0x10;
            p[6] = 0x80;
            sc = Sprite_Current;
            if (sc[2] == 3 && ((Frame_Counter >> 2) & 7) == number) {
                p[6] = 0x40;
                p[4] = 0x30;
                p[5] = 0x30;
                sc = Sprite_Current;
            }
            if (sc[2] >= 5) {
                const unsigned char fade = sc[0xA];
                p[5] = 0x10;
                p[4] = static_cast<unsigned char>(fade + 0x10);
                p[6] = static_cast<unsigned char>(0x80 - Sprite_Current[0xA]);
            }
            Gfx_PacketNext = Gfx_PacketNext + 0x38;
        }
    }
    unsigned base = 0;
    for (std::uint32_t lat = kShellAngles + 0x1C; lat < kShellAngles + 0x2C; lat += 8, base += 2) {
        unsigned n = 0;
        for (std::uint32_t lon = kShellAngles + 0xC; lon < kShellAngles + 0x24; lon += 0xC, ++n) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            ShellVertex(0, Table(lat), Table(lon - 0xC));
            ShellVertex(8, Table(lat), Table(lon));
            ShellVertex(0x10, Table(lat), Table(lon - 8));
            ShellVertex(0x18, Table(lat), Table(lon - 4));
            depths[12 + base + n] = MH_CALL(Gte_RotAverage4)(V(0), V(8), V(0x10), V(0x18), F(p + 8), F(p + 0x14),
                                                            F(p + 0x20), F(p + 0x2C), &projected);
            p[4] = 1;
            p[5] = 1;
            p[6] = 0x80;
            sc = Sprite_Current;
            if (sc[2] >= 5) {
                const unsigned char fade = sc[0xA];
                p[5] = 1;
                p[4] = static_cast<unsigned char>(fade + 1);
                p[6] = static_cast<unsigned char>(0x80 - Sprite_Current[0xA]);
            }
            Gfx_PacketNext = Gfx_PacketNext + 0x38;
        }
    }
    sc = Sprite_Current;
    MH_AT(SortFn, kSortPrims)(static_cast<std::uint32_t>(Long(sc + 0x34)), static_cast<std::uint32_t>(Long(sc + 0x38)),
                              depths, first, 0x10, 0x38, 2);
    sc = Sprite_Current;
    MH_AT(SortFn, kSortPrims)(static_cast<std::uint32_t>(Long(sc + 0x34)), static_cast<std::uint32_t>(Long(sc + 0x38)),
                              line_depths, line_base, 0xC, 0x38, 2);
    Gfx_PacketNext = lines;
}

// original 0x4BE550: one edge of the shell: a semi-transparent flat line
// quad at `prim` (0x40, 0x40, 0xC0) through four SVECTORs passed by value,
// its depth (Gte_RotAverage4's result) to *out. The original also passes the
// address of its own first argument as a tenth, which Gte_RotAverage4 does
// not read.
MS17_EXPORT void __cdecl LeechShell_Edge(unsigned char* prim, long* out, unsigned long a0, unsigned long a1,
                                         unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5,
                                         unsigned long a6, unsigned long a7) {
    MH_CALL(Gpu_SetLineF4)(prim);
    MH_CALL(Gpu_SetSemiTrans)(prim, 1);
    prim[4] = 0x40;
    prim[5] = 0x40;
    prim[6] = 0xC0;
    const unsigned long v[8] = {a0, a1, a2, a3, a4, a5, a6, a7};
    const short* const s = reinterpret_cast<const short*>(v);
    long projected;
    *out = MH_CALL(Gte_RotAverage4)(s, s + 4, s + 8, s + 12, F(prim + 8), F(prim + 0x14), F(prim + 0x20), F(prim + 0x2C),
                                    &projected);
}

// original 0x4BE5B0: pushes the matrix and loads Camera_Matrix x the
// shell's: translation RotTrans of (x >> 9 - 0x4000, z >> 9 - 0x4000,
// -(height / 2)), rotation 0 but about z the spin +0xC when +5 (the task's
// parameter) is 0xD. MagicFx_PushActorMatrix with a spin; pops nothing.
MS17_EXPORT void __cdecl LeechShell_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short rot[4] = {0, 0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    if (sc[5] == 0xD) rot[2] = S16(sc + 0xC);
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

// original 0x4BE660: the orb. Between two draw modes (tpage 0x35, slot 4):
// once started (+2 not 0) its rings under the actor matrix; then a call
// through LeechOrb_Phases 0x65B228 by +2, read in place (six entries).
MS17_EXPORT void __cdecl LeechOrb_Task(void) {
    ModeCommit4();
    if (Sprite_Current[2] != 0) {
        MH_CALL(MagicFx_PushActorMatrix)();
        MH_CALL(LeechOrb_DrawRings)();
        MH_CALL(Gte_PopMatrix)();
    }
    Entry(kOrbPhases, Sprite_Current[2])();
    ModeCommit4();
}

// original 0x4BE6D0: held until the effect's task (+0x4C) has +0xB 1 (the
// shell has aimed); then +0xA 0, +2 on.
MS17_EXPORT void __cdecl LeechOrb_WaitShell(void) {
    unsigned char* const sc = Sprite_Current;
    if (Pointer(Key(sc + 0x4C))[0xB] != 1) return;
    sc[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4BE6F0: +9 up to 8; there sound 0x101 and +2 on.
MS17_EXPORT void __cdecl LeechOrb_Chime(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 8) {
        MH_CALL(Sound_PlayById)(0x101);
        Bump(Sprite_Current[2]);
        return;
    }
    Bump(sc[9]);
}

// original 0x4BE720: toward the shell (0x4FB9F0, speed 0x30), 0x10 higher a
// frame; once within 0x1C000 of it (0x4FBBD0), +0xA 30 and +2 on.
MS17_EXPORT void __cdecl LeechOrb_Rise(void) {
    MH_AT(SeekFn, kSeek)(Owner(), 0x30);
    SetWord(Sprite_Current + 0x3E, Word(Sprite_Current + 0x3E) + 0x10u);
    if (MH_AT(NearFn, kNear)(Owner(), 0x1C000) == 0) return;
    Sprite_Current[0xA] = 0x1E;
    Bump(Sprite_Current[2]);
}

// original 0x4BE770: +0xA down; at 0 sound 0x102 and +2 on.
MS17_EXPORT void __cdecl LeechOrb_Pause(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    MH_CALL(Sound_PlayById)(0x102);
    Bump(Sprite_Current[2]);
}

// original 0x4BE7A0: toward the shell (speed 8); once within 0x4000 of it,
// the effect's task (+0x4C, read before) +0xB 2, +0xA 0, +2 on.
MS17_EXPORT void __cdecl LeechOrb_Merge(void) {
    const unsigned char* const sc = Sprite_Current;
    unsigned char* const owner = Owner();
    unsigned char* const task = Pointer(Key(sc + 0x4C));
    MH_AT(SeekFn, kSeek)(owner, 8);
    if (MH_AT(NearFn, kNear)(Owner(), 0x4000) == 0) return;
    task[0xB] = 2;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4BE7F0: +0xA up; at 8 the orb freed (a tail jmp).
MS17_EXPORT void __cdecl LeechOrb_End(void) {
    Bump(Sprite_Current[0xA]);
    if (Sprite_Current[0xA] == 8) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BE810: eight rings of sixteen semi-transparent flat quads
// round the orb, latitudes 0x400..0xC00 by 0x100, longitudes by 0x100,
// radius +9 * 4 (dword 0x903850): the vertex (cos(lat) * r * sin(lon),
// cos(lat) * r * cos(lon), sin(lat) * r). Each quad a draw mode (tpage 0xB5)
// and the quad, both sorted at the orb moved by its far edge's x / y << 9;
// red (0x80, 0x10, 0x10), from phase 5 fading by +0xA.
MS17_EXPORT void __cdecl LeechOrb_DrawRings(void) {
    SetLong(Mem(kScratchA), Sprite_Current[9] << 2);
    int lat = 0x400;
    for (;;) {
        int c = Cos(lat);
        int t = M(c, R());
        int s = Sin(0);
        SetWord(VB(8), static_cast<unsigned>(M(t, s)));
        c = Cos(lat);
        t = M(c, R());
        s = Cos(0);
        SetWord(VB(0xA), static_cast<unsigned>(M(t, s)));
        s = Sin(lat);
        SetWord(VB(0xC), static_cast<unsigned>(M(s, R())));
        const int next = lat + 0x100;
        c = Cos(next);
        t = M(c, R());
        s = Sin(0);
        SetWord(VB(0x18), static_cast<unsigned>(M(t, s)));
        c = Cos(next);
        t = M(c, R());
        s = Cos(0);
        SetWord(VB(0x1A), static_cast<unsigned>(M(t, s)));
        s = Sin(next);
        SetWord(VB(0x1C), static_cast<unsigned>(M(s, R())));
        for (int lon = 0x100; lon < 0x1100; lon += 0x100) {
            CopyVertex(0, 8);
            c = Cos(lat);
            t = M(c, R());
            s = Sin(lon);
            SetWord(VB(8), static_cast<unsigned>(M(t, s)));
            c = Cos(lat);
            t = M(c, R());
            s = Cos(lon);
            SetWord(VB(0xA), static_cast<unsigned>(M(t, s)));
            s = Sin(lat);
            SetWord(VB(0xC), static_cast<unsigned>(M(s, R())));
            CopyVertex(0x10, 0x18);
            c = Cos(next);
            t = M(c, R());
            s = Sin(lon);
            SetWord(VB(0x18), static_cast<unsigned>(M(t, s)));
            c = Cos(next);
            t = M(c, R());
            s = Cos(lon);
            SetWord(VB(0x1A), static_cast<unsigned>(M(t, s)));
            s = Sin(next);
            SetWord(VB(0x1C), static_cast<unsigned>(M(s, R())));
            const unsigned char* sc = Sprite_Current;
            const std::uint32_t x = (static_cast<std::uint32_t>(static_cast<int>(S16(VB(0x18)))) << 9) +
                                    static_cast<std::uint32_t>(Long(sc + 0x34));
            const std::uint32_t z = (static_cast<std::uint32_t>(static_cast<int>(S16(VB(0x1A)))) << 9) +
                                    static_cast<std::uint32_t>(Long(sc + 0x38));
            DrawMode(0xB5);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(V(0), V(8), V(0x10), V(0x18), F(p + 8), F(p + 0x14), F(p + 0x20), F(p + 0x2C),
                                       &depth);
            MH_CALL(Gte_PrimDepths4_0C)(p);
            sc = Sprite_Current;
            if (sc[2] < 5) {
                p[4] = 0x80;
                p[5] = 0x10;
                p[6] = 0x10;
            } else {
                p[4] = static_cast<unsigned char>(0x81 - (sc[0xA] << 4));
                p[5] = static_cast<unsigned char>(0x11 - (Sprite_Current[0xA] << 1));
                p[6] = static_cast<unsigned char>(0x11 - (Sprite_Current[0xA] << 1));
            }
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x38);
        }
        lat = next;
        if (lat >= 0xC00) break;
    }
}

#undef MS17_EXPORT

void MagicS17_Inject() {
    if (bof3::WantsShadow("magic_s17")) magic_s17::SelfTest();
    BOF3_INJECT(Purify_Task);
    BOF3_INJECT(Purify_Start);
    BOF3_INJECT(Purify_Glow);
    BOF3_INJECT(Purify_Fade);
    BOF3_INJECT(PurifyMote_Dispatch);
    BOF3_INJECT(PurifyMote_Task);
    BOF3_INJECT(PurifyMote_Place);
    BOF3_INJECT(PurifyMote_End);
    BOF3_INJECT(Revive_Task);
    BOF3_INJECT(Revive_Start);
    BOF3_INJECT(Revive_TintSource);
    BOF3_INJECT(Revive_WaitMotes);
    BOF3_INJECT(Revive_Fade);
    BOF3_INJECT(Revive_IsStrong);
    BOF3_INJECT(ReviveHalo_Dispatch);
    BOF3_INJECT(ReviveHalo_Task);
    BOF3_INJECT(ReviveHalo_Wait);
    BOF3_INJECT(ReviveHalo_Spawn);
    BOF3_INJECT(ReviveHalo_Draw);
    BOF3_INJECT(ReviveMote_Dispatch);
    BOF3_INJECT(ReviveMote_Task);
    BOF3_INJECT(ReviveMote_Launch);
    BOF3_INJECT(ReviveMote_Rise);
    BOF3_INJECT(ReviveMote_Draw);
    BOF3_INJECT(ReviveMote_Alloc);
    BOF3_INJECT(Leech_Task);
    BOF3_INJECT(Leech_Start);
    BOF3_INJECT(Leech_WaitOrbs);
    BOF3_INJECT(LeechOrb_Dispatch);
    BOF3_INJECT(LeechShell_Task);
    BOF3_INJECT(LeechShell_Grow);
    BOF3_INJECT(MagicFx_WaitA);
    BOF3_INJECT(LeechShell_Aim);
    BOF3_INJECT(LeechShell_Hold);
    BOF3_INJECT(LeechShell_WaitOrb);
    BOF3_INJECT(LeechShell_Fade);
    BOF3_INJECT(LeechShell_End);
    BOF3_INJECT(LeechShell_Draw);
    BOF3_INJECT(LeechShell_Edge);
    BOF3_INJECT(LeechShell_PushMatrix);
    BOF3_INJECT(LeechOrb_Task);
    BOF3_INJECT(LeechOrb_WaitShell);
    BOF3_INJECT(LeechOrb_Chime);
    BOF3_INJECT(LeechOrb_Rise);
    BOF3_INJECT(LeechOrb_Pause);
    BOF3_INJECT(LeechOrb_Merge);
    BOF3_INJECT(LeechOrb_End);
    BOF3_INJECT(LeechOrb_DrawRings);
}
