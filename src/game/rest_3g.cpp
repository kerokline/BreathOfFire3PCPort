// Round fourteen group R3G (docs/rest_3g.md): the 32 functions of
// analysis/round14_cut.tsv's group R3G, 0x4925C0..0x5171FB, each read with
// capstone to its last instruction (2026-10-04). A mixed band - what round
// thirteen's effect groups left and the catalog's top-level rows between them:
//
//   kinds 0xAC, 0xAD, 0xAE, 0xBA   effect states the effect groups' tables
//                   point at (E4F's EffectKindAC/AD/AE/BA_States)
//   Screen_TriangleWinding   the z of a screen triangle's cross product
//   Battle_PlaceBossActors / _PlaceBossActor, BattleEnemy_ClearStates
//                   the battle's boss actors copied into the enemy records
//   GameMode8..11_*  four game modes' step dispatchers and steps
//   Quake_VertexLift  a BMAGIC cell vertex's height from Quake's lift table
//   Area109_SwitchHook / _SwitchPattern   area 109's switch cell
//   EffectKind18Sub41_DrawPanels / _DrawRings   sub-kind 0x41's two draws
//   AreaMap_FrameAreaBD, AreaMapBD_BuildView, AreaMapBD_CellTexture
//                   area 0xBD's view, built from a 16 x 16 block map
//   EffectKind0F_CharCount   a string's characters (a byte above 0x7F takes two)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call; a
// memory cell the original reads after a call is read after it here, each
// call's answer into a local first. No divergence: each is a faithful
// replacement. Where the original jumps through a step table past its end or
// indexes past a table, ours aborts with a message (docs/rest_3g.md section 7).
// AreaMapBD_BuildView reads its draw-item array, its item bound and its four
// cull bounds from the operands DIV-0062 and DIV-0041 patch, as the original
// does.
#include "game/rest_3g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3g::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(At(a), v); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S8(unsigned char b) { return static_cast<signed char>(b); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
// sar on the 32 bits.
U Sar(U v, unsigned n) { return static_cast<U>(I(v) >> n); }
float Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 (its low dword 0); the callers keep eax or its low word.
U Ftol(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}
// fld dword [from]; fstp dword [to]: the copy through the FPU, which quietens a
// signalling NaN where a mov would not.
void FpuCopy(void* to, const void* from) {
    float f;
    std::memcpy(&f, from, sizeof f);
    volatile long double through = f;
    const float back = static_cast<float>(through);
    std::memcpy(to, &back, sizeof back);
}

// xor eax, eax; mov ax, [Game_Step]; jmp [table + eax * 4]: the table's
// `entries` steps read in place (the fuzz swaps the cells for recorders); a
// Fatal past them, where the original jumps through the dword after - the next
// mode's table or data.
void StepDispatch(const char* who, U table, unsigned entries) {
    const unsigned step = Game_Step;
    if (step >= entries)
        bof3::Fatal("%s: Game_Step is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3g.md section 7)",
                    who, step, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * step)))();
}

// The two framed waits of the mode steps: while the wait word is not 0, a frame
// and a sleep; while File_LoadDone answers 0, `frame` (if any) and a sleep.
void WaitWord(void (*frame)()) {
    while (MoveScript_WaitWordDA != 0) {
        frame();
        SH_CALL(Task_Sleep)(1);
    }
}
void LoadingFrame() { SH_CALL(Field_LoadingFrame)(); }
void Mode8Frame() { SH_CALL(GameMode8_WaitFrame)(); }
// call File_LoadDone; test eax, eax; jne done; loop: [frame;] Task_Sleep(1);
// File_LoadDone; test; je loop.
void WaitLoad(void (*frame)()) {
    if (SH_CALL(File_LoadDone)() != 0) return;
    do {
        if (frame) frame();
        SH_CALL(Task_Sleep)(1);
    } while (SH_CALL(File_LoadDone)() == 0);
}

}  // namespace

// ===========================================================================
// Kind 0xAC: EffectKindAC_States (E4F's), three states - a fade in and out
// ===========================================================================

// original 0x4925C0 (state 0; hidden in R3F's 0x492400): +9 = 8, +1 up.
extern "C" void __cdecl EffectKindAC_Start(void) {
    S()[9] = 8;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x4925E0 (state 1): R3F's gradient 0x492400 with the shade
// s8(+9) * -0x20 (imul cl: ax; the push carries Sprite_Current's upper half
// above it), +9 down; at 0 +9 = 8 and +1 up. +9 runs 8..1, so the shade 0x00,
// 0x20, .., 0xE0.
extern "C" void __cdecl EffectKindAC_FadeIn(void) {
    {
        const unsigned char* const s = S();
        const U product = static_cast<U>(S8(s[9]) * -0x20) & 0xFFFFu;
        SH_AT(void(__cdecl*)(U), at::kFadeDraw)((AddressOf(s) & 0xFFFF0000u) | product);
    }
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] != 0) return;
    S()[9] = 8;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x492620 (state 2): the gradient with 0xFF while +9 is 8, else
// +9 << 5 (a byte, Sprite_Current's upper 24 bits above it); +9 down; at 0 a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKindAC_FadeOut(void) {
    {
        const unsigned char* const s = S();
        const unsigned char n = s[9];
        const U shade = n == 8 ? 0xFFu : (AddressOf(s) & 0xFFFFFF00u) | static_cast<unsigned char>(n << 5);
        SH_AT(void(__cdecl*)(U), at::kFadeDraw)(shade);
    }
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] == 0) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0xAD: EffectKindAD_States (E4F's), states 0 and 1 - a ring rising at
// the third party member
// ===========================================================================

// original 0x492680 (state 0): +0x34 / +0x38 the third member's x / z (ObjTrio
// record 2), +0x3C the ground there (AreaMap_Elevation's s16 << 16); the copy
// +0xC..+0x14; +0x5D 0, +0x5E 0x80, +9 0x20, +1 up.
extern "C" void __cdecl EffectKindAD_Start(void) {
    SetUL(S() + 0x34, UL(at::kMember2X));
    SetUL(S() + 0x38, UL(at::kMember2Z));
    const long h = SH_CALL(AreaMap_Elevation)(Long(S() + 0x34), Long(S() + 0x38));
    unsigned char* const s = S();
    SetUL(s + 0x3C, static_cast<U>(static_cast<std::int16_t>(h)) << 16);
    SetUL(s + 0xC, UL(s + 0x34));
    SetUL(s + 0x10, UL(s + 0x38));
    SetUL(s + 0x14, UL(s + 0x3C));
    s[0x5D] = 0;
    s[0x5E] = 0x80;
    s[9] = 0x20;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x492710 (state 1): +0x3C up 0x800000; EffectKindAD_DrawArc(+0x34,
// +0xC, +0x5D, +0x5E); once the counter byte 0x903848 is 0xB, +1 up.
extern "C" void __cdecl EffectKindAD_Rise(void) {
    {
        unsigned char* const s = S();
        SetUL(s + 0x3C, UL(s + 0x3C) + 0x800000u);
        SH_CALL(EffectKindAD_DrawArc)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<const long*>(s + 0xC),
                                      s[0x5D], s[0x5E]);
    }
    if (At(at::kCounter)[0] == 0xB) S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// ===========================================================================
// Kind 0xAE: EffectKindAE_States (E4F's), state 0
// ===========================================================================

// original 0x492780 (state 0): the point (0x188000, 0x120000) and its ground
// plus 0x800 (<< 16) at +0x34..+0x3C; +0x2E 0x80, +0x30 0x180, +0x32 0, +9 8,
// +1 up; the map camera; the point projected (a) and again 0x100 higher (b);
// +0x32 the angle Math_Ratan2(b.y - a.y, b.x - a.x) + 0x400 (each difference
// rounded to a float, as fstp leaves it); sound 0x202.
extern "C" void __cdecl EffectKindAE_Start(void) {
    SetUL(S() + 0x34, 0x188000u);
    SetUL(S() + 0x38, 0x120000u);
    const long h = SH_CALL(AreaMap_Elevation)(Long(S() + 0x34), Long(S() + 0x38));
    {
        unsigned char* const s = S();
        SetUL(s + 0x3C, static_cast<U>(static_cast<std::int16_t>(h) + 0x800) << 16);
        SetWord(s + 0x2E, 0x80);
        SetWord(s + 0x30, 0x180);
        SetWord(s + 0x32, 0);
        s[9] = 8;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    {
        const unsigned char* const s = S();
        point[0] = Long(s + 0x34);
        point[1] = Long(s + 0x38);
        point[2] = Long(s + 0x3C);
    }
    float a[3], b[3];
    SH_CALL(EffectGte_ProjectPoint)(point, a);
    point[2] = static_cast<long>(static_cast<U>(point[2]) - 0x1000000u);
    SH_CALL(EffectGte_ProjectPoint)(point, b);
    const float dx = static_cast<float>(static_cast<long double>(b[0]) - static_cast<long double>(a[0]));
    const float dy = static_cast<float>(static_cast<long double>(b[1]) - static_cast<long double>(a[1]));
    const int angle = SH_CALL(Math_Ratan2)(dy, dx);
    SetWord(S() + 0x32, static_cast<U>(angle) + 0x400u);
    SH_CALL(Sound_PlayEffect)(0x202);
}

// ===========================================================================
// Kind 0xBA: EffectKindBA_States (E4F's), state 1 - the line from extra
// sprite 1's ring
// ===========================================================================

// original 0x493E50 (state 1; hidden in EffectKindB9_DrawShard 0x493C60): from
// the angle +0x6C, d1 = ((3 sin - cos) * 3 << 13) sar 12 and d2 = ((-sin - 3
// cos) * 3 << 13) sar 12 (four Math calls, each with +0x6C read again); +0x34
// / +0x38 / +0x3C extra sprite 1's x + d1, z + d2, y + 0x1000000; +0xC its x
// - (s16 +0x2E << 15) + d1, +0x10 / +0x14 as +0x38 / +0x3C;
// EffectKindBA_DrawLine(+0x34, +0xC, 0x40); the word +0x2E up; at 0x14A sound
// 0x216 and +1 up.
extern "C" void __cdecl EffectKindBA_Line(void) {
    const int sin1 = SH_CALL(Math_Sin)(Long(S() + 0x6C));
    const U three_sin = static_cast<U>(sin1) * 3u;
    const int cos1 = SH_CALL(Math_Cos)(Long(S() + 0x6C));
    const U d1 = Sar(((three_sin - static_cast<U>(cos1)) * 3u) << 13, 12);
    const int cos2 = SH_CALL(Math_Cos)(Long(S() + 0x6C));
    const U three_cos = static_cast<U>(cos2) * 3u;
    const int sin2 = SH_CALL(Math_Sin)(Long(S() + 0x6C));
    const U d2 = Sar(((0u - static_cast<U>(sin2) - three_cos) * 3u) << 13, 12);
    {
        unsigned char* const s = S();
        SetUL(s + 0x34, UL(at::kExtra1X) + d1);
        SetUL(s + 0x38, UL(at::kExtra1Z) + d2);
        SetUL(s + 0x3C, UL(at::kExtra1Y) + 0x1000000u);
        SetUL(s + 0xC, UL(at::kExtra1X) - (static_cast<U>(S16(s + 0x2E)) << 15) + d1);
        SetUL(s + 0x10, UL(at::kExtra1Z) + d2);
        SetUL(s + 0x14, UL(at::kExtra1Y) + 0x1000000u);
        SH_CALL(EffectKindBA_DrawLine)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<const long*>(s + 0xC),
                                       0x40);
    }
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 1u);
    if (Word(S() + 0x2E) != 0x14A) return;
    SH_CALL(Sound_PlayEffect)(0x216);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// ===========================================================================
// A screen triangle's winding
// ===========================================================================

// original 0x4941B0 (cdecl; EffectKind44_RingCone, EyeBeam_DrawCylinder and
// Shisu_DrawModel test its ax): the z of (b - a) x (c - b) of three screen
// points, (c.y - b.y)(b.x - a.x) - (b.y - a.y)(c.x - b.x) on x87, through a
// tail jump to _ftol (truncated; edx:eax, eax answered).
extern "C" int __cdecl Screen_TriangleWinding(const float* a, const float* b, const float* c) {
    const long double first = (static_cast<long double>(c[1]) - static_cast<long double>(b[1])) *
                              (static_cast<long double>(b[0]) - static_cast<long double>(a[0]));
    const long double second = (static_cast<long double>(b[1]) - static_cast<long double>(a[1])) *
                               (static_cast<long double>(c[0]) - static_cast<long double>(b[0]));
    return static_cast<int>(Ftol(first - second));
}

// ===========================================================================
// The boss actors at a boss encounter
// ===========================================================================

// original 0x494500 (called first by Battle_InitBossEncounter 0x4942A0): the
// encounter row EventBattle_Records[0x904AAA] + 2 names; for each of its eight
// slots whose kind is not 0xFF, Battle_PlaceBossActor(slot, the count so far,
// the kind); 0x904AB2 0 before, copied to 0x904AB3 after (the actors placed).
// The original passes the slot and the count from stack bytes whose upper
// three bytes it never wrote; its callee reads their low bytes only.
extern "C" void __cdecl Battle_PlaceBossActors(void) {
    const unsigned event = At(at::kEventBattle)[0];
    const unsigned row = At(at::kEventRows + 4 * event)[0];
    At(at::kPlaced)[0] = 0;
    if (row >= at::kEncounterRows)
        bof3::Fatal("Battle_PlaceBossActors: event battle %u names encounter row %u, past Encounter_Rows' %u - the original "
                    "reads on into the enemy kinds' records (docs/rest_3g.md section 7)",
                    event, row, at::kEncounterRows);
    unsigned char count = 0;
    for (unsigned slot = 0; slot < 8; ++slot) {
        const unsigned kind = At(at::kEncounterRowsAt + 9 * row + slot)[0];
        if (kind == 0xFF) continue;
        SH_CALL(Battle_PlaceBossActor)(slot, count, kind);
        count = static_cast<unsigned char>(count + 1);
    }
    At(at::kPlacedTotal)[0] = At(at::kPlaced)[0];
}

// original 0x494570 (cdecl, by Battle_PlaceBossActors): the field actor tagged
// with the slot (BossActor_Find); none, nothing. Else its first 0x80 bytes into
// enemy record `count` (rep movsd), Sprite_Current = that record; +1..+4 0,
// +0x29 4, +5 count + 3, the dwords +0xC..+0x20 0, the bytes +0x5C..+0x5F, +6,
// +7, +0x2B 0, +8 the formation byte 0x904AAC ^ 2, +0x48 0;
// Battle_CopyEnemyData(count, kind), Battle_SetEnemyOffset(count, kind's byte);
// the record's +0x8F 0; the actor's +0 | 0x40; 0x904AB2 up.
extern "C" void __cdecl Battle_PlaceBossActor(unsigned slot, unsigned count, unsigned kind) {
    unsigned char* const actor = SH_CALL(BossActor_Find)(slot);
    if (!actor) return;
    const unsigned n = count & 0xFF;
    if (n >= at::kEnemyCount)
        bof3::Fatal("Battle_PlaceBossActor: count %u, past the %u enemy records - the original writes past them "
                    "(docs/rest_3g.md section 7)",
                    n, at::kEnemyCount);
    unsigned char* const e = At(at::kEnemies + at::kEnemyStride * n);
    for (unsigned i = 0; i < 0x80; i += 4) SetUL(e + i, UL(actor + i));
    Sprite_Current = e;
    unsigned char* const s = S();
    s[1] = 0;
    s[4] = 0;
    s[3] = 0;
    s[2] = 0;
    s[0x29] = 4;
    s[5] = static_cast<unsigned char>(count + 3);
    SetUL(s + 0x14, 0);
    SetUL(s + 0x10, 0);
    SetUL(s + 0xC, 0);
    SetUL(s + 0x20, 0);
    SetUL(s + 0x1C, 0);
    SetUL(s + 0x18, 0);
    s[0x5F] = 0;
    s[0x5E] = 0;
    s[0x5D] = 0;
    s[0x5C] = 0;
    s[6] = 0;
    s[7] = 0;
    s[0x2B] = 0;
    s[8] = static_cast<unsigned char>(At(at::kFormation)[0] ^ 2);
    s[0x48] = 0;
    SH_CALL(Battle_CopyEnemyData)(count, kind);
    SH_CALL(Battle_SetEnemyOffset)(count, kind & 0xFF);
    At(at::kEnemies + at::kEnemyStride * n + 0x8F)[0] = 0;
    actor[0] = static_cast<unsigned char>(actor[0] | 0x40);
    At(at::kPlaced)[0] = static_cast<unsigned char>(At(at::kPlaced)[0] + 1);
}

// original 0x494E70 (BattleEnd_Finish, BattleLoss_ResetParty, BattleLoss_Restart,
// Escape_Leave): bytes +0..+4 of the eight enemy records 0.
extern "C" void __cdecl BattleEnemy_ClearStates(void) {
    for (unsigned i = 0; i < at::kEnemyCount; ++i) {
        unsigned char* const e = At(at::kEnemies + at::kEnemyStride * i);
        for (unsigned k = 0; k < 5; ++k) e[k] = 0;
    }
}

// ===========================================================================
// Game modes 8..11 (GameMode_Handlers 8..11) and their steps
// ===========================================================================

// original 0x496430 (GameMode_Handlers[8]): jmp [GameMode8_Steps + Game_Step *
// 4], unchecked.
extern "C" void __cdecl GameMode8_Run(void) {
    StepDispatch("GameMode8_Run", AddressOf(GameMode8_Steps), GameMode8_Steps_count);
}

// original 0x496440 (GameMode8_Steps[0]): the dword 0x904144 up; Fish_Spawn;
// Transition_Start(1); AreaMap_Frame; Field_DrawFrame; a draw-area move of the
// rectangle (0, buffer * 0xF0 + 0x50, 0x40, 0x78) to (0x340, 0x100) committed
// at slot 5 (0x18); GameMode8_Frame; Field_ScriptFlags2 bit 6 clear, Game_Step
// up, Field_EdgeBits 0, Field_Request 0.
extern "C" void __cdecl GameMode8_Enter(void) {
    SetUL(at::kModeCounter, UL(at::kModeCounter) + 1u);
    SH_CALL(Fish_Spawn)();
    SH_CALL(Transition_Start)(1);
    SH_CALL(AreaMap_Frame)();
    SH_CALL(Field_DrawFrame)();
    alignas(4) unsigned char rect[8];
    SetWord(rect, 0);
    SetWord(rect + 2, static_cast<U>(Gfx_BufferIndex) * 0xF0u + 0x50u);
    SetWord(rect + 4, 0x40);
    SetWord(rect + 6, 0x78);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x340, 0x100);
    SH_CALL(Gfx_CommitPrim)(5, 0x18);
    SH_CALL(GameMode8_Frame)();
    SetWord(At(at::kFlags2), Word(At(at::kFlags2)) & 0xFFBFu);
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    SetWord(At(at::kEdgeBits), 0);
    Field_Request = 0;
}

namespace {
// Music_Track not 0xFF and not the byte 0x904CD0: the fade.
bool MusicChanges() {
    const unsigned char track = At(at::kMusicTrack)[0];
    return track != 0xFF && At(at::kMusicPlaying)[0] != track;
}
// 0x90412C |= 0x80 and the sound bank of its low seven bits + 0x2C2 loaded.
void LoadBank() {
    const unsigned char b = static_cast<unsigned char>(At(at::kBankByte)[0] | 0x80);
    At(at::kBankByte)[0] = b;
    SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C2u);
}
}  // namespace

// original 0x4964E0 (GameMode8_Steps[2]): Field_ScriptFlags2 bit 6 set; the
// sound bank (0x90412C) and its framed wait (GameMode8_WaitFrame, a sleep);
// Music_FadeOut(0x10) if the track changes; Transition_Start(0) and its framed
// wait on the wait word; Field_ChangeArea(the word 0x802290, the dwords
// 0x7E091C, 0x7E0920, 4); Music_FadeOutStop(0xA) if the track changes; Game_Mode
// 1, Game_Step 0.
extern "C" void __cdecl GameMode8_Leave(void) {
    {
        const unsigned char was = At(at::kBankByte)[0];
        At(at::kFlags2)[0] = static_cast<unsigned char>(At(at::kFlags2)[0] | 0x40);
        const unsigned char b = static_cast<unsigned char>(was | 0x80);
        At(at::kBankByte)[0] = b;
        SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C2u);
    }
    WaitLoad(&Mode8Frame);
    if (MusicChanges()) SH_CALL(Music_FadeOut)(0x10);
    SH_CALL(Transition_Start)(0);
    WaitWord(&Mode8Frame);
    {
        const U z = UL(at::kReturnZ);
        const U x = UL(at::kReturnX);
        const unsigned area = Word(At(at::kReturnArea));
        SH_CALL(Field_ChangeArea)(area, I(x), I(z), 4);
    }
    if (MusicChanges()) SH_CALL(Music_FadeOutStop)(0xA);
    Game_Mode = 1;
    Game_Step = 0;
}

// original 0x4965C0 (GameMode_Handlers[9]): jmp [GameMode9_Steps + Game_Step *
// 4], unchecked.
extern "C" void __cdecl GameMode9_Run(void) {
    StepDispatch("GameMode9_Run", AddressOf(GameMode9_Steps), GameMode9_Steps_count);
}

namespace {
// Modes 9 and 10's step 0 (0x4965D0, 0x4966F0, byte for byte but the file):
// Transition_Start(2) and its framed wait (Field_LoadingFrame); two sleeps; the
// file loaded and waited for (a sleep a try); the CLUT strip's rows 1 and 2
// copied; Game_Step up; Gfx_ClutStripDirty 1.
void MenuModeEnter(int file) {
    SH_CALL(Transition_Start)(2);
    WaitWord(&LoadingFrame);
    SH_CALL(Task_Sleep)(1);
    SH_CALL(Task_Sleep)(1);
    SH_CALL(LoadDatFile)(file);
    WaitLoad(nullptr);
    SH_CALL(Gfx_ClutStripCopyRow)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(2);
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    Gfx_ClutStripDirty = 1;
}
}  // namespace

// original 0x4965D0 (GameMode9_Steps[0]): the file 0xCB.
extern "C" void __cdecl GameMode9_Enter(void) { MenuModeEnter(0xCB); }

// original 0x496660 (GameMode9_Steps[2] and GameMode10_Steps[2]): two sleeps;
// the sound bank (0x90412C) loaded, not waited for; Transition_Start(3) and its
// framed wait (Field_LoadingFrame); Game_Mode 2, Game_Step 0, Field_Request 0;
// a tail jump to Field_Frame.
extern "C" void __cdecl GameMode9_Leave(void) {
    SH_CALL(Task_Sleep)(1);
    SH_CALL(Task_Sleep)(1);
    LoadBank();
    SH_CALL(Transition_Start)(3);
    WaitWord(&LoadingFrame);
    Game_Mode = 2;
    Game_Step = 0;
    Field_Request = 0;
    SH_CALL(Field_Frame)();
}

// original 0x4966E0 (GameMode_Handlers[10]): jmp [GameMode10_Steps + Game_Step
// * 4], unchecked.
extern "C" void __cdecl GameMode10_Run(void) {
    StepDispatch("GameMode10_Run", AddressOf(GameMode10_Steps), GameMode10_Steps_count);
}

// original 0x4966F0 (GameMode10_Steps[0]): the file 0x31A.
extern "C" void __cdecl GameMode10_Enter(void) { MenuModeEnter(0x31A); }

// original 0x496780 (GameMode_Handlers[11]): jmp [GameMode11_Steps + Game_Step
// * 4], unchecked.
extern "C" void __cdecl GameMode11_Run(void) {
    StepDispatch("GameMode11_Run", AddressOf(GameMode11_Steps), GameMode11_Steps_count);
}

// original 0x496790 (GameMode11_Steps[0]): Mode11_ObjectFrame, Mode11_FieldFrame;
// with Field_Request 0, Game_Mode 2.
extern "C" void __cdecl GameMode11_Frame(void) {
    SH_CALL(Mode11_ObjectFrame)();
    SH_CALL(Mode11_FieldFrame)();
    if (Field_Request == 0) Game_Mode = 2;
}

// original 0x4967B0 (GameMode11_Steps[1]): Look_PadControl; a tail jump to
// Mode11_FieldFrame.
extern "C" void __cdecl GameMode11_Look(void) {
    SH_CALL(Look_PadControl)();
    SH_CALL(Mode11_FieldFrame)();
}

// original 0x4967C0 (GameMode11_Steps[2]): Look_Return, Mode11_FieldFrame; with
// the yaw word back at 0xFD56 and the pitch word at 0x200, Game_Step 0.
extern "C" void __cdecl GameMode11_LookEnd(void) {
    SH_CALL(Look_Return)();
    SH_CALL(Mode11_FieldFrame)();
    if (Word(At(at::kYaw)) == 0xFD56 && Word(At(at::kPitch)) == 0x200) Game_Step = 0;
}

// ===========================================================================
// Quake's vertex lift (MapCell_DrawTexQuads, _DrawShadedQuads, _DrawSpinQuads)
// ===========================================================================

// original 0x4CF4B0 (cdecl; callers read ax): the vertex (x, z), each (v +
// 0x4000) sar 6 less twice the heaved block's first column / row (0x695C2C /
// 0x695C2E) - with Quake's facing bit 0 the axes swap: a from x, b from z;
// without, a from z, b from x. Outside a 0..0x1F, b 0..0x1B: ax 0 (the upper
// half a's). Else by the parities of a and b, from the lift table 0x695A2C
// (index i = (a >> 1) * 15 + (b >> 1)): both even -(s8 [i+1] + s8 [i+0xF]) << 3;
// b odd a even -(s8 [i+0x10] + s8 [i+1]) << 3; b even a odd -(s8 [i+0x10] + s8
// [i+0xF]) << 3; both odd -s8 [i+0x10] << 4.
extern "C" unsigned __cdecl Quake_VertexLift(long x, long z) {
    const U fx = Sar(static_cast<U>(x) + 0x4000u, 6) - (static_cast<U>(S16(At(at::kQuakeX))) << 1);
    const U fz = Sar(static_cast<U>(z) + 0x4000u, 6) - (static_cast<U>(S16(At(at::kQuakeY))) << 1);
    U a, b;   // eax and esi
    if (At(at::kQuakeFacing)[0] & 1) {
        b = fz;
        a = fx;
    } else {
        b = fx;
        a = fz;
    }
    if (I(a) >= 0x20 || I(b) >= 0x1C || I(a) < 0 || I(b) < 0) return a & 0xFFFF0000u;
    const U i = (Sar(a, 1) * 15u) + Sar(b, 1);
    auto t = [&](U offset) { return static_cast<U>(S8(At(at::kQuakeLast + offset + i)[0])); };
    if (((a | b) & 1) == 0) return (0u - (t(1) + t(0xF))) << 3;
    if ((b & 1) != 0 && (a & 1) == 0) return (0u - (t(0x10) + t(1))) << 3;
    if ((b & 1) == 0 && (a & 1) != 0) return (0u - (t(0x10) + t(0xF))) << 3;
    return (0u - t(0x10)) << 4;
}

// ===========================================================================
// Area 109's switch (Area_CellHooks' entry for area 0x6D)
// ===========================================================================

// original 0x4FEE70 (cdecl; EffectKind18_09_Pattern twice, Area109_SwitchHook):
// the three story flags 0x65DE60 lists as bits 0..2, plus 1 - eax 1..8.
extern "C" int __cdecl Area109_SwitchPattern(void) {
    U bits = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char set = SH_CALL(Flags_Test)(At(at::kStoryFlags), At(at::kPatternFlags + i)[0]);
        if (set != 0) bits |= 1u << i;
    }
    return static_cast<int>(bits + 1);
}

// original 0x4FEEB0 (Area_CellHook's hook for area 0x6D, (x, z) cell bytes,
// al): at cell (0x1C, 6) with the leader facing 3 and story flag 0x1C clear, the
// pattern (Area109_SwitchPattern) mod 6 (idiv) set as the three flags anew -
// each cleared, then set by its bit; Effect_HoldFlag1C(0xF); sounds 0x206,
// 0x202; al 1. Otherwise al 0 (the rest of eax the caller's).
extern "C" unsigned char __cdecl Area109_SwitchHook(long x, long z) {
    if ((static_cast<U>(x) & 0xFF) != 0x1C || (static_cast<U>(z) & 0xFF) != 6 || At(at::kLeaderFacing)[0] != 3) return 0;
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x1C) != 0) return 0;
    const int pattern = SH_CALL(Area109_SwitchPattern)();
    int bits = pattern % 6;
    for (unsigned i = 0; i < 3; ++i) {
        SH_CALL(Flags_Clear)(At(at::kStoryFlags), At(at::kPatternFlags + i)[0]);
        if (bits & 1) SH_CALL(Flags_Set)(At(at::kStoryFlags), At(at::kPatternFlags + i)[0]);
        bits >>= 1;
    }
    SH_CALL(Effect_HoldFlag1C)(0xF);
    SH_CALL(Sound_PlayEffect)(0x206);
    SH_CALL(Sound_PlayEffect)(0x202);
    return 1;
}

// ===========================================================================
// Kind 0x18 sub-kind 0x41's two draws (EffectKind18Sub41_Hold's)
// ===========================================================================

// original 0x5100B0 (cdecl): a draw mode (page 0x95) committed at slot 5
// (0xC); two textured quads from the eight three-byte vertices at 0x65ED30 -
// x = b0 << 7 - 0x37C0, z = b1 << 7 - 0x2DC0, y = -0x3F8 - b2 * lift (each a
// word) - projected (Gte_RotTransPers4, Gte_PrimDepths4_10), the texture word
// (texture << 16) | 0xBB009120, committed at slot 5 (0x48).
extern "C" void __cdecl EffectKind18Sub41_DrawPanels(unsigned lift, unsigned texture) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    for (U from = at::kPanelVertices; from < at::kPanelVerticesEnd; from += 12) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const v = Vertices();
        for (unsigned k = 0; k < 4; ++k) {
            const unsigned char* const b = At(from + 3 * k);
            SetWord(v + 8 * k, (static_cast<U>(b[0]) << 7) - 0x37C0u);
            SetWord(v + 8 * k + 2, (static_cast<U>(b[1]) << 7) - 0x2DC0u);
            SetWord(v + 8 * k + 4, 0xFFFFFC08u - static_cast<U>(b[2]) * lift);
        }
        long depth;
        const short* const vs = Prim_VertexScratch;
        SH_CALL(Gte_RotTransPers4)(vs, vs + 4, vs + 8, vs + 12, reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)((texture << 16) | 0xBB009120u, p, 1);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
    }
}

// original 0x5101C0 (cdecl, with +0xA): a draw mode (dtd, page 0xB5) committed
// at slot 5 (0xC); the vertex word z 0xFC08 (once). Three rings, from r = the
// argument and c = (0x28 - it) * 5, each next with r - 0x10 and c + 0x10: r
// below 0 taken as 0, c clamped to 0..0xFF; R = (r << 12) sar 10. Each: a
// semi-transparent LINE_F2 coloured (c / 2, c / 2, c) from (0xC840, R -
// 0x2CC0) to (0xC9C0, R - 0x2CC0); 16 chords of the circle of radius r about
// (-13888, -11456) - each end (sin * r sar 10 + x, cos * r sar 10 + y) through
// _ftol, coloured c / 2 - at angles 0..0x400 by 0x40; a line from (R - 0x3640,
// 0xD240) to (R - 0x3640, 0xD340); 16 chords about (-13888, -11712) at
// 0x400..0x800. Each line projected end by end (Gte_RotTransPers, the depth
// stored) and committed at slot 5 (0x20). The draw mode again (no dtd) to end.
extern "C" void __cdecl EffectKind18Sub41_DrawRings(int size) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    unsigned char* const v = Vertices();
    const short* const v0 = Prim_VertexScratch;
    SetWord(v + 4, 0xFC08);
    U radius_arg = static_cast<U>(size);
    U colour = (0x28u - static_cast<U>(size)) * 5u;
    long p_out;
    // A line's two ends projected into p + 8 / p + 0x14, each depth after it.
    auto end = [&](unsigned char* p, unsigned at_xy, unsigned at_depth) {
        SH_CALL(Gte_RotTransPers)(v0, reinterpret_cast<unsigned long*>(p + at_xy), &p_out);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + at_depth));
    };
    for (unsigned ring = 3; ring != 0; --ring) {
        const U r = I(radius_arg) < 0 ? 0u : radius_arg;
        U c = colour;
        if (I(c) > 0xFF) c = 0xFF;
        const U c8 = I(c) < 0 ? 0u : c;
        const unsigned char half = static_cast<unsigned char>(c8 >> 1);
        const U big = Sar(r << 12, 10);
        // the straight line across
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetLineF2)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[6] = static_cast<unsigned char>(c8);
            p[4] = half;
            p[5] = half;
            SetWord(v, 0xC840);
            SetWord(v + 2, big - 0x2CC0u);
            end(p, 8, 0x10);
            SetWord(v, 0xC9C0);
            SetWord(v + 2, big - 0x2CC0u);
            end(p, 0x14, 0x1C);
            SH_CALL(Gfx_CommitPrim)(5, 0x20);
        }
        // a circle's 16 chords about the centre MapView_ScreenXY holds
        auto chords = [&](U from, U to) {
            for (U angle = from; angle < to;) {
                unsigned char* const p = Gfx_PacketNext;
                SH_CALL(Gpu_SetLineF2)(p);
                SH_CALL(Gpu_SetSemiTrans)(p, 1);
                p[4] = half;
                p[5] = half;
                p[6] = half;
                for (unsigned k = 0; k < 2; ++k) {
                    if (k == 1) angle += 0x40;
                    const int s = SH_CALL(Math_Sin)(static_cast<int>(angle));
                    const U dx = Sar(static_cast<U>(s) * r, 10);
                    SetWord(v, Ftol(static_cast<long double>(I(dx)) + static_cast<long double>(Fl(AddressOf(MapView_ScreenXY)))));
                    const int co = SH_CALL(Math_Cos)(static_cast<int>(angle));
                    const U dy = Sar(static_cast<U>(co) * r, 10);
                    SetWord(v + 2,
                            Ftol(static_cast<long double>(I(dy)) + static_cast<long double>(Fl(AddressOf(MapView_ScreenXY) + 4))));
                    end(p, k == 0 ? 8 : 0x14, k == 0 ? 0x10 : 0x1C);
                }
                SH_CALL(Gfx_CommitPrim)(5, 0x20);
            }
        };
        SetUL(AddressOf(MapView_ScreenXY), 0xC6590000u);       // -13888.0
        SetUL(AddressOf(MapView_ScreenXY) + 4, 0xC6330000u);   // -11456.0
        chords(0, 0x400);
        // the straight line down
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetLineF2)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[6] = static_cast<unsigned char>(c8);
            p[4] = half;
            p[5] = half;
            SetWord(v, big - 0x3640u);
            SetWord(v + 2, 0xD240);
            end(p, 8, 0x10);
            SetWord(v, big - 0x3640u);
            SetWord(v + 2, 0xD340);
            end(p, 0x14, 0x1C);
            SH_CALL(Gfx_CommitPrim)(5, 0x20);
        }
        SetUL(AddressOf(MapView_ScreenXY), 0xC6590000u);       // -13888.0
        SetUL(AddressOf(MapView_ScreenXY) + 4, 0xC6370000u);   // -11712.0
        chords(0x400, 0x800);
        radius_arg -= 0x10;
        colour += 0x10;
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// ===========================================================================
// Area 0xBD's view
// ===========================================================================

// original 0x510630 (AreaMap_Frame's whole work in area 0xBD): MapView_FocusX
// / Z = (0x800000 - Field_Kind2X / Z) sar 8; with any of the three angle words
// moved from Camera_AnglesDrawn, MapView_Redraw 3 and both dwords copied;
// Gte_RotMatrix(Camera_Angles, Camera_Matrix); the vector (((FocusX >> 1) -
// 0x4000) & 0x7FFF) - 0x4000, the same of FocusZ, MapView_Elevation >> 1 (each
// a word, read after the matrix) through Gte_ApplyMatrix, plus Camera_ShiftX,
// Camera_ShiftY and Camera_Distance + 0x1194, as the translation; the matrix
// set; with MapView_Redraw, AreaMapBD_BuildView and MapView_Redraw down.
extern "C" void __cdecl AreaMap_FrameAreaBD(void) {
    MapView_FocusX = static_cast<long>(Sar(0x800000u - static_cast<U>(Field_Kind2X), 8));
    MapView_FocusZ = static_cast<long>(Sar(0x800000u - static_cast<U>(Field_Kind2Z), 8));
    {
        unsigned char* const angles = reinterpret_cast<unsigned char*>(Camera_Angles);
        unsigned char* const drawn = reinterpret_cast<unsigned char*>(Camera_AnglesDrawn);
        const U first = UL(angles), second = UL(angles + 4);
        if (Word(angles) != Word(drawn) || Word(angles + 2) != Word(drawn + 2) || (second & 0xFFFFu) != Word(drawn + 4)) {
            MapView_Redraw = 3;
            SetUL(drawn, first);
            SetUL(drawn + 4, second);
        }
    }
    SH_CALL(Gte_RotMatrix)(Camera_Angles, Camera_Matrix);
    short vector[3];
    vector[0] = static_cast<short>((((static_cast<U>(MapView_FocusX) >> 1) - 0x4000u) & 0x7FFFu) - 0x4000u);
    vector[1] = static_cast<short>((((static_cast<U>(MapView_FocusZ) >> 1) - 0x4000u) & 0x7FFFu) - 0x4000u);
    vector[2] = static_cast<short>(static_cast<U>(MapView_Elevation) >> 1);
    long moved[3];
    SH_CALL(Gte_ApplyMatrix)(Camera_Matrix, vector, moved);
    unsigned char* const matrix = reinterpret_cast<unsigned char*>(Camera_Matrix);
    SetUL(matrix + 0x14, static_cast<U>(static_cast<int>(Camera_ShiftX)) + static_cast<U>(moved[0]));
    SetUL(matrix + 0x18, static_cast<U>(static_cast<int>(Camera_ShiftY)) + static_cast<U>(moved[1]));
    SetUL(matrix + 0x1C, static_cast<U>(static_cast<int>(Camera_Distance)) + static_cast<U>(moved[2]) + 0x1194u);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    if (MapView_Redraw == 0) return;
    SH_CALL(AreaMapBD_BuildView)();
    MapView_Redraw = static_cast<unsigned char>(MapView_Redraw - 1);
}

namespace {
// The float a patched `fcomp dword ptr [mem]` operand names.
float Bound(U operand_at) { return Fl(UL(operand_at)); }
}  // namespace

// original 0x510780 (AreaMap_FrameAreaBD's, with MapView_Redraw): area 0xBD's
// cells drawn. DrawTable_Count 0; this buffer's lists 0 and 1 of the 0x38
// DrawLayers emptied; the camera's octant ((Cond_AngleFB + 0x100) sar 9) & 7
// picks a record of six s8 at 0x65ED48 (x0, dx per column, dx per row, z0, dz
// per column, dz per row); the first cell x0 - (byte (FocusX >> 8) - 0x80) +
// 0x100 and z0 likewise (words 0x903850 / 0x903852 and MapView_Origin); an odd
// octant walks 0x38 rows of 0x1C cells, an even one 0x2C of 0x28 (dwords
// 0x903854 / 0x903858); DrawItemPool_Top 0. Each cell, while the item index is
// below its bound (0x400): x = row * dxr / 2 + col * dxc + x0, z = (row + 1) *
// dzr / 2 + col * dzc + z0 (the halves toward 0); the block map 0x65ED78
// [((x sar 4) & 0xF) | (z & 0xF0)] gives the map cell (row nibble << 4 | z &
// 0xF) * 0x60 + (column nibble << 4 | x & 0xF); its first corner projected; kept
// when its screen y is above 120 and x inside (-200, 520), or y below 121 (or
// unordered) and x inside (-50, 370); a kept cell takes the next draw item
// (DrawItemPool_Top up), its texture (AreaMapBD_CellTexture), its first screen
// point (copied through the FPU), its three other corners projected, linked
// into list 0 of layer 0x37 - row, and its depths.
extern "C" void __cdecl AreaMapBD_BuildView(void) {
    DrawTable_Count = 0;
    {
        unsigned char* layer = At(AddressOf(DrawLayers) + Gfx_BufferIndex * 8u);
        for (unsigned n = 0; n < 0x38; ++n, layer += 0x30) {
            unsigned char* list = layer;
            for (unsigned k = 0; k < 2; ++k, list += 0x10) {
                SetUL(list, 0);
                SetUL(list + 4, AddressOf(list));
            }
        }
    }
    const U octant = Sar(static_cast<U>(Cond_AngleFB) + 0x100u, 9) & 7u;
    const U fx = ((static_cast<U>(MapView_FocusX) >> 8) - 0x80u) & 0xFFu;
    const U fz = ((static_cast<U>(MapView_FocusZ) >> 8) - 0x80u) & 0xFFu;
    const unsigned char* const rec = At(at::kOctants + octant * 6);
    const U even = (~octant) & 1u;   // not eax; and eax, 1
    const U x0 = static_cast<U>(S8(rec[0])) - fx + 0x100u;
    const U z0 = static_cast<U>(S8(rec[3])) - fz + 0x100u;
    const U rows = 0x38u - even * 12u, columns = even * 12u + 0x1Cu;
    SetWord(At(at::kOriginX), x0);
    MapView_Origin[0] = static_cast<short>(x0);
    SetWord(At(at::kOriginZ), z0);
    MapView_Origin[1] = static_cast<short>(z0);
    SetUL(at::kRows, rows);
    SetUL(at::kColumns, columns);
    DrawItemPool_Top = 0;
    U row_bound = rows;
    for (U row = 0; I(row) < I(row_bound); ++row) {
        U col_bound = row == 0 ? columns : UL(at::kColumns);
        for (U col = 0; I(col) < I(col_bound); ++col, col_bound = UL(at::kColumns)) {
            if (DrawItemPool_Top >= Word(At(at::kTopBoundAt))) continue;
            const U a = static_cast<U>(S8(rec[2])) * row;
            const U x = static_cast<U>(I(a) / 2) + static_cast<U>(S8(rec[1])) * col + static_cast<U>(S16(At(at::kOriginX)));
            const U b = static_cast<U>(S8(rec[5])) * (row + 1u);
            const U z = static_cast<U>(I(b) / 2) + static_cast<U>(S8(rec[4])) * col + static_cast<U>(S16(At(at::kOriginZ)));
            const unsigned char block = At(at::kBlocks + ((Sar(x, 4) & 0xFu) | (z & 0xF0u)))[0];
            const U map_row = (block & 0xF0u) | (z & 0xFu);
            const U map_col = ((block & 0xFu) << 4) | (x & 0xFu);
            const U cell = map_row * at::kMapWidth + map_col;
            unsigned char* const v = Vertices();
            SetWord(v + 2, (z << 7) - 0x4040u);
            SetWord(v, (x << 7) - 0x4040u);
            unsigned char* corner = reinterpret_cast<unsigned char*>(&AreaMap_Corners) + cell * 4;
            MapView_CornerPtr = corner;
            SetWord(v + 4, (0u - static_cast<U>(S8(corner[0]))) << 4);
            SH_CALL(Gte_LoadVertex)(reinterpret_cast<const unsigned long*>(Prim_VertexScratch));
            SH_CALL(Gte_Rtps)();
            SH_CALL(Gte_StoreScreenXY)(reinterpret_cast<unsigned long*>(MapView_ScreenXY));
            // x87 fcomp against the operands' floats: an unordered compare sets
            // C0 and C3 both.
            const float sx = Fl(AddressOf(MapView_ScreenXY)), sy = Fl(AddressOf(MapView_ScreenXY) + 4);
            bool keep = sy > Bound(at::kWideYAt) && sx > Bound(at::kWideLoAt) && sx < Bound(at::kWideHiAt);
            if (!keep) {
                const bool y_ok = !(sy >= Bound(at::kNarrowYAt));
                keep = y_ok && sx > Bound(at::kNarrowLoAt) && !(sx >= Bound(at::kNarrowHiAt));
            }
            if (!keep) continue;

            const unsigned top = DrawItemPool_Top;
            unsigned char* const item = At(UL(at::kItemsAt) + top * 0x90u);
            DrawItemPool_Top = static_cast<unsigned short>(top + 1);
            {
                const U offset = Word(AreaMap_Header + 2);
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                const U tile = Word(AreaMap_Header + (cell + offset * 2u) * 2u);
                const U header = UL(AreaMap_Header);
                const U product = ((header >> 8) & 0xFFu) * (header & 0xFFu) + 1u;
                const U index = static_cast<U>(I(product) / 2) + offset + tile;
                SH_CALL(AreaMapBD_CellTexture)(static_cast<unsigned long>(UL(AreaMap_Header + index * 4u)), quad);
            }
            FpuCopy(item + Gfx_BufferIndex * 0x48u + 8, MapView_ScreenXY);
            FpuCopy(item + Gfx_BufferIndex * 0x48u + 0xC, MapView_ScreenXY + 1);
            {
                const U xy = UL(v);
                const U y = Word(v + 2);
                corner = MapView_CornerPtr;
                const U x1 = xy + 0x80u;
                SetWord(v + 8, x1);
                SetWord(v + 0xA, y);
                SetWord(v + 0xC, (0u - static_cast<U>(S8(static_cast<unsigned char>(UL(corner) >> 8))) ) << 4);
                const U y1 = y + 0x80u;
                SetWord(v + 0x10, Word(v));
                SetWord(v + 0x12, y1);
                SetWord(v + 0x18, x1);
                SetWord(v + 0x14, (0u - static_cast<U>(S8(static_cast<unsigned char>(UL(corner) >> 16))) ) << 4);
                SetWord(v + 0x1A, y1);
                SetWord(v + 0x1C, (0u - static_cast<U>(S8(static_cast<unsigned char>(UL(corner) >> 24))) ) << 4);
            }
            SH_CALL(Gte_LoadVertices3)(reinterpret_cast<const unsigned long*>(v + 8));
            SH_CALL(Gte_Rtpt)();
            {
                const U last = at::kLayerLast + (Gfx_BufferIndex - row * 6u) * 8u;
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                SH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(UL(last))), AddressOf(quad));
            }
            SetUL(at::kLayerLast + (Gfx_BufferIndex - row * 6u) * 8u, AddressOf(item + Gfx_BufferIndex * 0x48u));
            {
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                SH_CALL(Gte_StoreScreenXY3)(reinterpret_cast<unsigned long*>(quad + 0x18), reinterpret_cast<unsigned long*>(quad + 0x28),
                                            reinterpret_cast<unsigned long*>(quad + 0x38));
            }
            SH_CALL(Gte_PrimDepths4_10)(item + Gfx_BufferIndex * 0x48u);
        }
        row_bound = UL(at::kRows);
    }
}

// original 0x510BB0 (cdecl, by AreaMapBD_BuildView): the quad not semi-
// transparent; its colour 0x80 x 3; its CLUT word ((texture sar 24) & 0xF) +
// 0x1E3 << 6, its page word 0x95; the 16 x 16 tile of the texture's low byte -
// u (texture & 0xF) << 4 to + 0xF, v texture & 0xF0 to + 0xF.
extern "C" void __cdecl AreaMapBD_CellTexture(unsigned long texture, unsigned char* quad) {
    SH_CALL(Gpu_SetSemiTrans)(quad, 0);
    quad[6] = 0x80;
    quad[5] = 0x80;
    quad[4] = 0x80;
    const U t = static_cast<U>(texture);
    SetWord(quad + 0x26, 0x95);
    SetWord(quad + 0x16, ((Sar(t, 24) & 0xFu) + 0x1E3u) << 6);
    const unsigned char v0 = static_cast<unsigned char>(t & 0xF0u);
    const unsigned char u0 = static_cast<unsigned char>((t & 0xFu) << 4);
    quad[0x15] = v0;
    quad[0x25] = v0;
    quad[0x14] = u0;
    quad[0x24] = static_cast<unsigned char>(u0 + 0xF);
    quad[0x34] = u0;
    quad[0x35] = static_cast<unsigned char>(v0 + 0xF);
    quad[0x44] = static_cast<unsigned char>(u0 + 0xF);
    quad[0x45] = static_cast<unsigned char>(v0 + 0xF);
}

// ===========================================================================
// Kind 0xF's character count
// ===========================================================================

// original 0x5171E0 (cdecl; EffectKind0F_Title, _TitleClose, _LineType): the
// characters of a string to its NUL, a byte with bit 7 taking the next byte
// with it (that byte not tested for the NUL); eax.
extern "C" int __cdecl EffectKind0F_CharCount(const unsigned char* text) {
    int count = 0;
    unsigned char c = *text++;
    if (c == 0) return 0;
    do {
        if (c & 0x80) ++text;
        c = *text;
        ++count;
        ++text;
    } while (c != 0);
    return count;
}

void Rest3G_Inject() {
    if (bof3::WantsShadow("rest_3g")) rest_3g::SelfTest();
    BOF3_INJECT(EffectKindAC_Start);
    BOF3_INJECT(EffectKindAC_FadeIn);
    BOF3_INJECT(EffectKindAC_FadeOut);
    BOF3_INJECT(EffectKindAD_Start);
    BOF3_INJECT(EffectKindAD_Rise);
    BOF3_INJECT(EffectKindAE_Start);
    BOF3_INJECT(EffectKindBA_Line);
    BOF3_INJECT(Screen_TriangleWinding);
    BOF3_INJECT(Battle_PlaceBossActors);
    BOF3_INJECT(Battle_PlaceBossActor);
    BOF3_INJECT(BattleEnemy_ClearStates);
    BOF3_INJECT(GameMode8_Run);
    BOF3_INJECT(GameMode8_Enter);
    BOF3_INJECT(GameMode8_Leave);
    BOF3_INJECT(GameMode9_Run);
    BOF3_INJECT(GameMode9_Enter);
    BOF3_INJECT(GameMode9_Leave);
    BOF3_INJECT(GameMode10_Run);
    BOF3_INJECT(GameMode10_Enter);
    BOF3_INJECT(GameMode11_Run);
    BOF3_INJECT(GameMode11_Frame);
    BOF3_INJECT(GameMode11_Look);
    BOF3_INJECT(GameMode11_LookEnd);
    BOF3_INJECT(Quake_VertexLift);
    BOF3_INJECT(Area109_SwitchPattern);
    BOF3_INJECT(Area109_SwitchHook);
    BOF3_INJECT(EffectKind18Sub41_DrawPanels);
    BOF3_INJECT(EffectKind18Sub41_DrawRings);
    BOF3_INJECT(AreaMap_FrameAreaBD);
    BOF3_INJECT(AreaMapBD_BuildView);
    BOF3_INJECT(AreaMapBD_CellTexture);
    BOF3_INJECT(EffectKind0F_CharCount);
}
