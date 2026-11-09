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
// cull bounds as DIV-0062 (draw_pool.h) and DIV-0041 (widescreen.h) hold the
// operands they patch, as the original reads the operands
// (docs/operand-store.md).
#include "game/rest_3g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g_callees.h"
#include "game/cheats.h"
#include "game/map_rim.h"
#include "game/scenario_harness.h"
#include "game/draw_pool.h"
#include "game/psp_code.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
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
using HandlerRest3g = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U ULRest3g(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U ULRest3g(U a) { return ULRest3g(move_script::At(a)); }
void SetULRest3g(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetULRest3g(U a, U v) { SetULRest3g(At(a), v); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S8Rest3g(unsigned char b) { return static_cast<signed char>(b); }
U AddressOf(const void* p) { return Low32(p); }
// sar on the 32 bits.
U SarRest3g(U v, unsigned n) { return static_cast<U>(I(v) >> n); }
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 (its low dword 0); the callers keep eax or its low word.
U FtolRest3g(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// xor eax, eax; mov ax, [Game_Step]; jmp [table + eax * 4]: the table's
// `entries` steps read in place (the fuzz swaps the cells for recorders); a
// Fatal past them, where the original jumps through the dword after - the next
// mode's table or data.
void StepDispatchRest3g(const char* who, U table, unsigned entries) {
    const unsigned step = Game_Step;
    if (step >= entries)
        bof3::Fatal("%s: Game_Step is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3g.md section 7)",
                    who, step, entries, (unsigned)table);
    reinterpret_cast<HandlerRest3g>(static_cast<std::uintptr_t>(ULRest3g(table + 4 * step)))();
}

// The two framed waits of the mode steps: while the wait word is not 0, a frame
// and a sleep; while File_LoadDone answers 0, `frame` (if any) and a sleep.
void WaitWord(void (*frame)()) {
    while (Transition_Busy != 0) {
        frame();
        SH_CALL(Task_Sleep)(1);
    }
}
void LoadingFrame() { SH_CALL(Field_LoadingFrame)(); }
void Mode8Frame() { SH_CALL(GameMode8_WaitFrame)(); }
// call File_LoadDone; test eax, eax; jne done; loop: [frame;] Task_Sleep(1);
// File_LoadDone; test; je loop.
void WaitLoadRest3g(void (*frame)()) {
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

// original 0x4925E0 (state 1): R3F's fill 0x492400 (EffectKindAA_DrawFill,
// ours, widened under DIV-0041 - so this fade is too) with the shade
// s8(+9) * -0x20 (imul cl: ax; the push carries Sprite_Current's upper half
// above it), +9 down; at 0 +9 = 8 and +1 up. +9 runs 8..1, so the shade 0x00,
// 0x20, .., 0xE0.
extern "C" void __cdecl EffectKindAC_FadeIn(void) {
    {
        const unsigned char* const s = S();
        const U product = static_cast<U>(S8Rest3g(s[9]) * -0x20) & 0xFFFFu;
        SH_AT(void(__cdecl*)(U), at::kFadeDraw.v)((AddressOf(s) & 0xFFFF0000u) | product);
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
        SH_AT(void(__cdecl*)(U), at::kFadeDraw.v)(shade);
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
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->x, static_cast<std::uint32_t>(ObjTrio[2].x));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->z, static_cast<std::uint32_t>(ObjTrio[2].z));
    const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(S())->x, rv::As<records::EffectObject>(S())->z);
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->height = static_cast<U>(static_cast<std::int16_t>(h)) << 16;
    rv::As<records::EffectWork>(s)->work_0c = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x);
    rv::As<records::EffectWork>(s)->work_10 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z);
    SetULRest3g(s + 0x14, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
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
        rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 0x800000u;
        SH_CALL(EffectKindAD_DrawArc)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<const long*>(s + 0xC),
                                      s[0x5D], s[0x5E]);
    }
    if (At(::bof3::Img(at::kCounter.v))[0] == 0xB) S()[1] = static_cast<unsigned char>(S()[1] + 1);
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
    rv::As<records::EffectObject>(S())->x = 0x188000u;
    rv::As<records::EffectObject>(S())->z = 0x120000u;
    const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(S())->x, rv::As<records::EffectObject>(S())->z);
    {
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->height = static_cast<U>(static_cast<std::int16_t>(h) + 0x800) << 16;
        rv::As<records::EffectObject>(s)->screen_x = 0x80;
        rv::As<records::EffectObject>(s)->screen_y = 0x180;
        rv::As<records::EffectWork>(s)->work_32 = 0;
        s[9] = 8;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    {
        const unsigned char* const s = S();
        point[0] = rv::As<records::EffectObject>(s)->x;
        point[1] = rv::As<records::EffectObject>(s)->z;
        point[2] = rv::As<records::EffectObject>(s)->height;
    }
    float a[3], b[3];
    SH_CALL(EffectGte_ProjectPoint)(point, a);
    point[2] = static_cast<long>(static_cast<U>(point[2]) - 0x1000000u);
    SH_CALL(EffectGte_ProjectPoint)(point, b);
    const float dx = static_cast<float>(static_cast<long double>(b[0]) - static_cast<long double>(a[0]));
    const float dy = static_cast<float>(static_cast<long double>(b[1]) - static_cast<long double>(a[1]));
    const int angle = SH_CALL(Math_Ratan2)(dy, dx);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, static_cast<U>(angle) + 0x400u);
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
    const int sin1 = SH_CALL(Math_Sin)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U three_sin = static_cast<U>(sin1) * 3u;
    const int cos1 = SH_CALL(Math_Cos)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U d1 = SarRest3g(((three_sin - static_cast<U>(cos1)) * 3u) << 13, 12);
    const int cos2 = SH_CALL(Math_Cos)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U three_cos = static_cast<U>(cos2) * 3u;
    const int sin2 = SH_CALL(Math_Sin)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U d2 = SarRest3g(((0u - static_cast<U>(sin2) - three_cos) * 3u) << 13, 12);
    {
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->x = static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].x) + d1;
        rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].z) + d2;
        rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].height) + 0x1000000u;
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].x) - (static_cast<U>(S16(s + 0x2E)) << 15) + d1);
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].z) + d2);
        SetULRest3g(s + 0x14, static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].height) + 0x1000000u);
        SH_CALL(EffectKindBA_DrawLine)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<const long*>(s + 0xC),
                                       0x40);
    }
    rv::As<records::EffectObject>(S())->screen_x = static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->screen_x) + 1u;
    if (static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->screen_x) != 0x14A) return;
    SH_CALL(Sound_PlayEffect)(0x216);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// ===========================================================================
// A screen triangle's winding
// ===========================================================================

// original 0x4941B0 (PSX 0x801B0F88; cdecl; EffectKind44_RingCone, EyeBeam_DrawCylinder and
// Shisu_DrawModel test its ax): the z of (b - a) x (c - b) of three screen
// points, (c.y - b.y)(b.x - a.x) - (b.y - a.y)(c.x - b.x) on x87, through a
// tail jump to _ftol (truncated; edx:eax, eax answered).
extern "C" int __cdecl Screen_TriangleWinding(const float* a, const float* b, const float* c) {
    const long double first = (static_cast<long double>(c[1]) - static_cast<long double>(b[1])) *
                              (static_cast<long double>(b[0]) - static_cast<long double>(a[0]));
    const long double second = (static_cast<long double>(b[1]) - static_cast<long double>(a[1])) *
                               (static_cast<long double>(c[0]) - static_cast<long double>(b[0]));
    return static_cast<int>(FtolRest3g(first - second));
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
    const unsigned event = EventBattle_Id;
    const unsigned row = At(::bof3::Img(at::kEventRows.v) + 4 * event)[0];
    Battle_EnemyCount = 0;
    if (row >= at::kEncounterRows)
        bof3::Fatal("Battle_PlaceBossActors: event battle %u names encounter row %u, past Encounter_Rows' %u - the original "
                    "reads on into the enemy kinds' records (docs/rest_3g.md section 7)",
                    event, row, at::kEncounterRows);
    unsigned char count = 0;
    for (unsigned slot = 0; slot < 8; ++slot) {
        const unsigned kind = At(::bof3::Img(Encounter_Rows_addr) + 9 * row + slot)[0];
        if (kind == 0xFF) continue;
        SH_CALL(Battle_PlaceBossActor)(slot, count, kind);
        count = static_cast<unsigned char>(count + 1);
    }
    Battle_EnemiesLeft = Battle_EnemyCount;
}

// original 0x494570 (cdecl, by Battle_PlaceBossActors): the field actor tagged
// with the slot (BossActor_Find); none, nothing. Else its first 0x80 bytes into
// enemy record `count` (rep movsd), Sprite_Current = that record; +1..+4 0,
// +0x29 4, +5 count + 3, the dwords +0xC..+0x20 0, the bytes +0x5C..+0x5F, +6,
// +7, +0x2B 0, +8 the formation byte 0x904AAC ^ 2, +0x48 0;
// Battle_CopyEnemyData(count, kind), Battle_SetEnemyOffset(count, kind's byte);
// the record's +0x8F 0; the actor's +0 | 0x40; 0x904AB2 up.
//
// DIV-0187 (src/game/cheats.cpp): under BOF3X_ENEMY_INFO=1 the record's +0x8F
// is 1 instead of 0, so a boss shows its HP gauge and its Identify stats as
// a beaten kind does.
//
// DIV-0108 (psp_code.h): with the PSP's myria-item on, in area 198 under
// event battle 0x37 the record's item word +0xA8 is 0x317 after
// Battle_CopyEnemyData - the PSP's added store (PSP-JP 0x141570..0x141598,
// its +0x98), which reads the area and the event byte after that call, as
// here. Off, nothing.
extern "C" void __cdecl Battle_PlaceBossActor(unsigned slot, unsigned count, unsigned kind) {
    unsigned char* const actor = SH_CALL(BossActor_Find)(slot);
    if (!actor) return;
    const unsigned n = count & 0xFF;
    if (n >= at::kEnemyCount)
        bof3::Fatal("Battle_PlaceBossActor: count %u, past the %u enemy records - the original writes past them "
                    "(docs/rest_3g.md section 7)",
                    n, at::kEnemyCount);
    unsigned char* const e = At(::bof3::Img(Battle_Enemies_addr) + at::kEnemyStride * n);
    for (unsigned i = 0; i < 0x80; i += 4) SetULRest3g(e + i, ULRest3g(actor + i));
    Sprite_Current = e;
    unsigned char* const s = S();
    s[1] = 0;
    s[4] = 0;
    s[3] = 0;
    s[2] = 0;
    s[0x29] = 4;
    s[5] = static_cast<unsigned char>(count + 3);
    rv::As<records::BattleEnemy>(s)->step_y = 0;
    rv::As<records::BattleEnemy>(s)->step_z = 0;
    rv::As<records::BattleEnemy>(s)->step_x = 0;
    rv::As<records::BattleEnemy>(s)->work_20 = 0;
    rv::As<records::BattleEnemy>(s)->work_1c = 0;
    rv::As<records::BattleEnemy>(s)->work_18 = 0;
    s[0x5F] = 0;
    s[0x5E] = 0;
    s[0x5D] = 0;
    s[0x5C] = 0;
    s[6] = 0;
    s[7] = 0;
    s[0x2B] = 0;
    s[8] = static_cast<unsigned char>(At(::bof3::Img(at::kFormation.v))[0] ^ 2);
    s[0x48] = 0;
    SH_CALL(Battle_CopyEnemyData)(count, kind);
    psp_code::MyriaItem(At(::bof3::Img(Battle_Enemies_addr) + at::kEnemyStride * n), Game_AreaNumber, EventBattle_Id);
    SH_CALL(Battle_SetEnemyOffset)(count, kind & 0xFF);
    At(::bof3::Img(Battle_Enemies_addr) + at::kEnemyStride * n + 0x8F)[0] = Cheats_EnemyInfoShown() ? 1 : 0;   // DIV-0187
    actor[0] = static_cast<unsigned char>(actor[0] | 0x40);
    Battle_EnemyCount = static_cast<unsigned char>(Battle_EnemyCount + 1);
}

// original 0x494E70 (PSX 0x800A9EB0; BattleEnd_Finish, BattleLoss_ResetParty, BattleLoss_Restart,
// Escape_Leave): bytes +0..+4 of the eight enemy records 0.
extern "C" void __cdecl BattleEnemy_ClearStates(void) {
    for (unsigned i = 0; i < at::kEnemyCount; ++i) {
        unsigned char* const e = At(::bof3::Img(Battle_Enemies_addr) + at::kEnemyStride * i);
        for (unsigned k = 0; k < 5; ++k) e[k] = 0;
    }
}

// ===========================================================================
// Game modes 8..11 (GameMode_Handlers 8..11) and their steps
// ===========================================================================

// original 0x496430 (GameMode_Handlers[8]): jmp [GameMode8_Steps + Game_Step *
// 4], unchecked.
extern "C" void __cdecl GameMode8_Run(void) {
    StepDispatchRest3g("GameMode8_Run", AddressOf(GameMode8_Steps), GameMode8_Steps_count);
}

// original 0x496440 (GameMode8_Steps[0]; PSX 0x801994E8): the dword 0x904144 up; Fish_Spawn;
// Transition_Start(1); AreaMap_Frame; Field_DrawFrame; a draw-area move of the
// rectangle (0, buffer * 0xF0 + 0x50, 0x40, 0x78) to (0x340, 0x100) committed
// at slot 5 (0x18); GameMode8_Frame; Field_ScriptFlags2 bit 6 clear, Game_Step
// up, Field_StepCount 0, Field_Request 0.
extern "C" void __cdecl GameMode8_Enter(void) {
    Rest4a_CountF = Rest4a_CountF + 1u;
    SH_CALL(Fish_Spawn)();
    SH_CALL(Transition_Start)(1);
    SH_CALL(AreaMap_Frame)();
    SH_CALL(Field_DrawFrame)();
    alignas(4) unsigned char rect[8];
    move_script::SetWord(rect, 0);
    move_script::SetWord(rect + 2, static_cast<U>(Gfx_BufferIndex) * 0xF0u + 0x50u);
    move_script::SetWord(rect + 4, 0x40);
    move_script::SetWord(rect + 6, 0x78);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x340, 0x100);
    SH_CALL(Gfx_CommitPrim)(5, 0x18);
    SH_CALL(GameMode8_Frame)();
    Field_ScriptFlags2 = Field_ScriptFlags2 & 0xFFBFu;
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    Field_StepCount = 0;
    Field_Request = 0;
}

namespace {
// Music_Track not 0xFF and not the byte 0x904CD0: the fade.
bool MusicChanges() {
    const unsigned char track = Music_Track;
    return track != 0xFF && AreaW1f_MusicCurrent != track;
}
// 0x90412C |= 0x80 and the sound bank of its low seven bits + 0x2C2 loaded.
void LoadBank() {
    const unsigned char b = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
    AreaW0b_PartySet = b;
    SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C2u);
}
}  // namespace

// original 0x4964E0 (GameMode8_Steps[2]; PSX 0x801995C8): Field_ScriptFlags2 bit 6 set; the
// sound bank (0x90412C) and its framed wait (GameMode8_WaitFrame, a sleep);
// Music_FadeOut(0x10) if the track changes; Transition_Start(0) and its framed
// wait on the wait word; Field_ChangeArea(the word 0x802290, the dwords
// 0x7E091C, 0x7E0920, 4); Music_FadeOutStop(0xA) if the track changes; Game_Mode
// 1, Game_Step 0.
extern "C" void __cdecl GameMode8_Leave(void) {
    {
        const unsigned char was = AreaW0b_PartySet;
        move_script::At(::bof3::Img(Field_ScriptFlags2_addr))[0] = static_cast<unsigned char>(move_script::At(::bof3::Img(Field_ScriptFlags2_addr))[0] | 0x40);
        const unsigned char b = static_cast<unsigned char>(was | 0x80);
        AreaW0b_PartySet = b;
        SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C2u);
    }
    WaitLoadRest3g(&Mode8Frame);
    if (MusicChanges()) SH_CALL(Music_FadeOut)(0x10);
    SH_CALL(Transition_Start)(0);
    WaitWord(&Mode8Frame);
    {
        const U z = ULRest3g(::bof3::Img(at::kReturnZ.v));
        const U x = ULRest3g(::bof3::Img(at::kReturnX.v));
        const unsigned area = AreaW2c_LastArea;
        SH_CALL(Field_ChangeArea)(area, I(x), I(z), 4);
    }
    if (MusicChanges()) SH_CALL(Music_FadeOutStop)(0xA);
    Game_Mode = 1;
    Game_Step = 0;
}

// original 0x4965C0 (GameMode_Handlers[9]): jmp [GameMode9_Steps + Game_Step *
// 4], unchecked.
extern "C" void __cdecl GameMode9_Run(void) {
    StepDispatchRest3g("GameMode9_Run", AddressOf(GameMode9_Steps), GameMode9_Steps_count);
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
    WaitLoadRest3g(nullptr);
    SH_CALL(Gfx_ClutStripCopyRow)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(2);
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    Gfx_ClutStripDirty = 1;
}
}  // namespace

// original 0x4965D0 (GameMode9_Steps[0]; PSX 0x80199780): the file 0xCB.
extern "C" void __cdecl GameMode9_Enter(void) { MenuModeEnter(0xCB); }

// original 0x496660 (GameMode9_Steps[2] and GameMode10_Steps[2]; PSX 0x8019983C): two sleeps;
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
    StepDispatchRest3g("GameMode10_Run", AddressOf(GameMode10_Steps), GameMode10_Steps_count);
}

// original 0x4966F0 (GameMode10_Steps[0]; PSX 0x80199940): the file 0x31A.
extern "C" void __cdecl GameMode10_Enter(void) { MenuModeEnter(0x31A); }

// original 0x496780 (GameMode_Handlers[11]): jmp [GameMode11_Steps + Game_Step
// * 4], unchecked.
extern "C" void __cdecl GameMode11_Run(void) {
    StepDispatchRest3g("GameMode11_Run", AddressOf(GameMode11_Steps), GameMode11_Steps_count);
}

// original 0x496790 (GameMode11_Steps[0]; PSX 0x80199B00): Mode11_ObjectFrame, Mode11_FieldFrame;
// with Field_Request 0, Game_Mode 2.
extern "C" void __cdecl GameMode11_Frame(void) {
    SH_CALL(Mode11_ObjectFrame)();
    SH_CALL(Mode11_FieldFrame)();
    if (Field_Request == 0) Game_Mode = 2;
}

// original 0x4967B0 (GameMode11_Steps[1]; PSX 0x80199B44): Look_PadControl; a tail jump to
// Mode11_FieldFrame.
extern "C" void __cdecl GameMode11_Look(void) {
    SH_CALL(Look_PadControl)();
    SH_CALL(Mode11_FieldFrame)();
}

// original 0x4967C0 (GameMode11_Steps[2]; PSX 0x80199B6C): Look_Return, Mode11_FieldFrame; with
// the yaw word back at 0xFD56 and the pitch word at 0x200, Game_Step 0.
extern "C" void __cdecl GameMode11_LookEnd(void) {
    SH_CALL(Look_Return)();
    SH_CALL(Mode11_FieldFrame)();
    if (static_cast<std::uint16_t>(Camera_Angles[0]) == 0xFD56 && move_script::Word(move_script::At(::bof3::Img(Cond_AngleFB_addr))) == 0x200) Game_Step = 0;
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
    const U fx = SarRest3g(static_cast<U>(x) + 0x4000u, 6) - (static_cast<std::uint32_t>(MagicS23_QuakeX) << 1);
    const U fz = SarRest3g(static_cast<U>(z) + 0x4000u, 6) - (static_cast<std::uint32_t>(MagicS23_QuakeY) << 1);
    U a, b;   // eax and esi
    if (MagicS23_QuakeFacing & 1) {
        b = fz;
        a = fx;
    } else {
        b = fx;
        a = fz;
    }
    if (I(a) >= 0x20 || I(b) >= 0x1C || I(a) < 0 || I(b) < 0) return a & 0xFFFF0000u;
    const U i = (SarRest3g(a, 1) * 15u) + SarRest3g(b, 1);
    auto t = [&](U offset) { return static_cast<U>(S8Rest3g(At(::bof3::Img(at::kQuakeLast.v) + offset + i)[0])); };
    if (((a | b) & 1) == 0) return (0u - (t(1) + t(0xF))) << 3;
    if ((b & 1) != 0 && (a & 1) == 0) return (0u - (t(0x10) + t(1))) << 3;
    if ((b & 1) == 0 && (a & 1) != 0) return (0u - (t(0x10) + t(0xF))) << 3;
    return (0u - t(0x10)) << 4;
}

// ===========================================================================
// Area 109's switch (Area_CellHooks' entry for area 0x6D)
// ===========================================================================

// ===========================================================================
// Kind 0x18 sub-kind 0x41's two draws (EffectKind18Sub41_Hold's)
// ===========================================================================

// ===========================================================================
// Area 0xBD's view
// ===========================================================================

namespace {
// The two y bounds' operands, which nothing patches: the .rdata floats 120.0
// and 121.0 they name - the function's own, ours since the function is.
constexpr U kWideYFloat = 0x5C4244, kNarrowYFloat = 0x5C4238;
}  // namespace

// ===========================================================================
// Kind 0xF's character count
// ===========================================================================

void Rest3G_Inject() {
    bof3::OperandExpect("AreaMapBD_BuildView y bound", at::kWideYAt.v, &kWideYFloat, 4);
    bof3::OperandExpect("AreaMapBD_BuildView y bound", at::kNarrowYAt.v, &kNarrowYFloat, 4);
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
    // DIV-0187 writes a boss's +0x8F here; Capcom's Battle_PlaceBossActor
    // would clear it and the cheat would miss every boss.
    if (Cheats_EnemyInfoShown() && !bof3::IsEnabled(bof3::addr::Battle_PlaceBossActor))
        bof3::Fatal("BOF3X_ENEMY_INFO=1 needs Battle_PlaceBossActor ours, and it is disabled");
}
