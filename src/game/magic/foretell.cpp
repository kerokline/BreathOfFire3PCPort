// Moved here by tools/move_funcs.py from battle_items.cpp, magic_s15.cpp (docs/source-layout-round.md).
#include "game/magic/foretell.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_items.h"
#include "game/battle_items_callees.h"
#include "game/magic_harness.h"
#include "game/magic_s15.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/sound.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

using namespace battle_items;

namespace battle_items {

using move_script::Long;

using move_script::Word;

}  // namespace battle_items

namespace {

short S16BattleItems(const unsigned char* at) { return static_cast<short>(battle_items::Word(at)); }

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3 (a party index lands below the enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

bool IsOut(unsigned actor) { return (MH_CALL(Battle_ActorIsOut)(actor & 0xFF) & 0xFFu) != 0; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

}  // namespace

#define S15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// Word a x 100 / word b, as the original's `idiv` (both zero-extended, so the
// quotient is never negative); a zero divisor faults in the original.
int Percent(const unsigned char* record, unsigned a, unsigned b, const char* side, unsigned i) {
    const int den = Word(record + b);
    if (den == 0) bof3::Fatal("Foretell_Read: %s %u's word +0x%X is 0, the original's idiv faults", side, i, b);
    return static_cast<int>(Word(record + a)) * 100 / den;
}

// The band a ratio falls in: `zero` for 0, else the last of `bands` (the
// values for > 0, > 10, .. > 70) whose bound it passes; a negative ratio keeps
// the previous step (the original's register carried over; never reached, as
// the ratio is never negative).
int Band(int q, int previous, int zero, const int (&bands)[8]) {
    if (q == 0) return zero;
    int step = previous;
    for (int k = 0; k < 8; ++k)
        if (q > k * 10) step = bands[k];
    return step;
}

void PushMessage(unsigned id) {
    const unsigned char* const text = MH_CALL(Msg_SystemPtr)(id);
    MH_CALL(BattleQueue_Push)(1, 0x3C, static_cast<unsigned long>(Key(text)));
}

}  // namespace

// original 0x4B7400: the kind-2 task. A five-entry stack table by +1:
// Foretell_Start, _Read, _Pause, _Show, _End.
S15_EXPORT void __cdecl Foretell_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Foretell_Start, bof3::addr::Foretell_Read,
                                                 bof3::addr::Foretell_Pause, bof3::addr::Foretell_Show,
                                                 bof3::addr::Foretell_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("Foretell_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B7440: the owner's direction and position (+0x34 / +0x38 /
// +0x3C); one child (kind 1, 0x1C, +1 0, +0x80 this task); +0xB 1, +1 on.
S15_EXPORT void __cdecl Foretell_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    const unsigned slot = NewTask(0x1C);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(s));
    child[1] = 0;
    s[0xB] = 1;
    Inc(Sc()[1]);
}

// original 0x4B74C0: once the child has ended (+0xB 0). A score from the two
// sides: each present member (+0 set; +0x91 bit 0x40 takes 0x18 off and is not
// counted) adds a step by its word +0x98 x 100 / word +0xA0, and each enemy
// not out one by its word +0xA4 x 100 / word +0xB0; then (0x904AB2 - the
// enemies counted) x 8, a band of the difference of the members' mean byte
// +0x8A and the enemies' mean word +0x98 (16-bit), and Foretell_Bias by the
// count difference. The score picks one of the messages 0x3C..0x40
// (Msg_SystemPtr) queued (BattleQueue_Push(1, 0x3C, text)). Then +0xB gets a
// bit for each of six record bytes (+0xBF, +0xC0, +0xC1, +0xC3, +0xC2 at most
// 1, +0xC4 at most 3) of any enemy not out; +9 0x16, +0xA 4, +1 on. A zero
// count or divisor faults in the original; ours aborts.
S15_EXPORT void __cdecl Foretell_Read(void) {
    if (Sc()[0xB] != 0) return;
    static constexpr int kPartyBands[8] = {-0x15, -0x12, -0xF, -0xC, -9, -6, -3, 0};
    static constexpr int kEnemyBands[8] = {0x15, 0x12, 0xF, 0xC, 9, 8, 3, 0};
    std::uint32_t score = 0;
    int step = 0;
    unsigned party_n = 0;
    std::uint16_t party_sum = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char* const r = PartyRecord(i);
        if (r[0] == 0) continue;
        if (r[0x91] & 0x40) {
            score -= 0x18;
            continue;
        }
        party_sum = static_cast<std::uint16_t>(party_sum + r[0x8A]);
        ++party_n;
        step = Band(Percent(r, 0x98, 0xA0, "member", i), step, -0x18, kPartyBands);
        score += static_cast<std::uint32_t>(step);
    }
    unsigned enemy_n = 0;
    std::uint16_t enemy_sum = 0;
    for (unsigned i = 0; i < 8; ++i) {
        if (IsOut(i + 3)) continue;
        const unsigned char* const e = EnemyRecord(i + 3);
        const records::BattleEnemy* const te = rv::AsConst<records::BattleEnemy>(e);
        enemy_sum = static_cast<std::uint16_t>(enemy_sum + te->level);
        ++enemy_n;
        step = Band(Percent(e, 0xA4, 0xB0, "enemy", i), step, 0x18, kEnemyBands);
        score += static_cast<std::uint32_t>(step);
    }
    score += (static_cast<std::uint32_t>(Mem(::bof3::Img(Battle_EnemyCount_addr))[0]) - enemy_n) * 8u;
    if (party_n == 0) bof3::Fatal("Foretell_Read: no member counted, the original's idiv faults");
    if (enemy_n == 0) bof3::Fatal("Foretell_Read: no enemy counted, the original's idiv faults");
    const int mean_party = static_cast<short>(party_sum) / static_cast<int>(party_n);
    const int mean_enemy = static_cast<short>(enemy_sum) / static_cast<int>(enemy_n);
    const short diff = static_cast<short>(mean_party - mean_enemy);
    static constexpr struct {
        short at_most;
        int value;
    } kLevels[] = {{20, 0x78}, {15, 0x6E}, {10, 0x64}, {7, 0x5F}, {5, 0x5A}, {3, 0x55}, {1, 0x50},
                   {-1, 0x46}, {-3, 0x3C}, {-5, 0x2D}, {-7, 0x1E}, {-10, 0x14}, {-15, 0xA}, {-20, 5}};
    int level = 0x82;
    for (const auto& l : kLevels)
        if (diff <= l.at_most) level = l.value;
    const int bias = static_cast<signed char>(
        Mem(::bof3::Img(Foretell_Bias_addr) + 0x7 + static_cast<std::uint32_t>(static_cast<int>(party_n) - static_cast<int>(enemy_n)))[0]);
    score += static_cast<std::uint32_t>(bias + level);
    short s16 = static_cast<short>(score);
    if (s16 < 0) {
        PushMessage(0x40);
    } else {
        if (s16 > 100) s16 = 100;
        if (s16 > 0x5A) PushMessage(0x3C);
        else if (s16 > 0x46) PushMessage(0x3D);
        else if (s16 > 0x32) PushMessage(0x3E);
        else if (s16 > 0x1E) PushMessage(0x3F);
        else PushMessage(0x40);
    }
    for (unsigned i = 0; i < 8; ++i) {
        if (IsOut(i + 3)) continue;
        const unsigned char* const e = EnemyRecord(i + 3);
        if (e[0xBF] <= 1) Sc()[0xB] |= 1;
        if (e[0xC0] <= 1) Sc()[0xB] |= 2;
        if (e[0xC1] <= 1) Sc()[0xB] |= 4;
        if (e[0xC3] <= 1) Sc()[0xB] |= 8;
        if (e[0xC2] <= 1) Sc()[0xB] |= 0x10;
        if (e[0xC4] <= 3) Sc()[0xB] |= 0x20;
    }
    Sc()[9] = 0x16;
    Sc()[0xA] = 4;
    Inc(Sc()[1]);
}

// original 0x4B7880: +0xA down; at 0 +0xA 0x3E and +1 on.
S15_EXPORT void __cdecl Foretell_Pause(void) {
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Sc()[0xA] = 0x3E;
    Inc(Sc()[1]);
}

// original 0x4B78B0: the tiles; a button pressed (Input_Pressed) sends +1 to
// 4.
S15_EXPORT void __cdecl Foretell_Show(void) {
    Call0(bof3::addr::Foretell_DrawTiles);
    if (Input_Pressed != 0) Sc()[1] = 4;
}

// original 0x4B78D0: with the message window down (0x939F60 0) the effect's
// done flag and the task freed; else while +9 is above 0 (signed) +9 down by
// 8 and the tiles drawn.
S15_EXPORT void __cdecl Foretell_End(void) {
    if (Mem(::bof3::Img(BattleE1_MessageUp_addr))[0] == 0) {
        Mem(::bof3::Img(magic_harness::at::kFlags))[0] |= 4;
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    unsigned char* const s = Sc();
    const auto nine = static_cast<signed char>(s[9]);
    if (nine <= 0) return;
    s[9] = static_cast<unsigned char>(nine - 8);
    Call0(bof3::addr::Foretell_DrawTiles);
}

// original 0x4B7900: a draw-mode packet (tpage 0x15) to layer 0; then from
// x 0xE8 (dword 0x903850), y +9 (dword 0x903854) a flat 8 x 12 tile (Gpu_SetTile)
// for each of +0xB's six bits that is set, coloured by Foretell_TileColours,
// 10 further right each; none set: one grey (0x60) tile. Layer 0.
S15_EXPORT void __cdecl Foretell_DrawTiles(void) {
    DrawModeCommit(0x15, 0);
    unsigned char* s = Sc();
    SetSD(0, 0xE8);
    SetSD(4, s[9]);
    constexpr std::int32_t kW = 0x41000000;   // 8.0f
    constexpr std::int32_t kH = 0x41400000;   // 12.0f
    for (unsigned k = 0; k < 6; ++k) {
        if (((1u << k) & s[0xB]) == 0) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetTile)(p);
        PutFloat(p + 8, SD(0));
        PutFloat(p + 0xC, SD(4));
        SetSD(0, static_cast<std::uint32_t>(SD(0)) + 10u);
        SetLong(p + 0x14, kW);
        SetLong(p + 0x18, kH);
        const unsigned char* const colour = Mem(::bof3::Img(Foretell_TileColours_addr) + 6 * k);
        p[4] = colour[0];
        p[5] = colour[2];
        p[6] = colour[4];
        MH_CALL(Gfx_CommitPrim)(0, 0x1C);
        s = Sc();
    }
    if (s[0xB] != 0) return;
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetTile)(p);
    PutFloat(p + 8, SD(0));
    SetLong(p + 0x14, kW);
    SetLong(p + 0x18, kH);
    p[4] = 0x60;
    p[5] = 0x60;
    PutFloat(p + 0xC, SD(4));
    p[6] = 0x60;
    MH_CALL(Gfx_CommitPrim)(0, 0x1C);
}

// original 0x4B7A10: the child's kind-1 task, a jmp through
// ForetellChild_Kinds (one entry, ForetellOrb_Run) by +1, unchecked.
S15_EXPORT void __cdecl ForetellChild_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("ForetellChild_Task", phase, 1);
    magic_harness::Phase(bof3::addr::ForetellOrb_Run)();
}

// original 0x4B7A30: a jmp through ForetellOrb_Steps (four entries) by +2,
// unchecked.
S15_EXPORT void __cdecl ForetellOrb_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::ForetellOrb_Start, bof3::addr::ForetellOrb_Spin,
                                                bof3::addr::ForetellOrb_Count, bof3::addr::ForetellOrb_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ForetellOrb_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B7A50: the owner's direction and position, 0x200 higher (+0x3C
// + 0x2000000); the screen point (BattleActor_UpdateScreenXY); sound 0x100;
// +0xB 0, +9 0xF, +0xA 3, +2 on.
S15_EXPORT void __cdecl ForetellOrb_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + 0x2000000u));
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0xF;
    Sc()[0xA] = 3;
    Inc(Sc()[2]);
}

// original 0x4B7AE0: the cell drawn under the actor matrix; +9 down; at 0 +9
// 0xF and +2 on.
S15_EXPORT void __cdecl ForetellOrb_Spin(void) {
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::ForetellOrb_Draw);
    MH_CALL(Gte_PopMatrix)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0xF;
    Inc(Sc()[2]);
}

// original 0x4B7B20: +9 down; at 0 +9 0xF and the count +0xA down: not yet 0,
// sound 0x100 and +2 back one; at 0 sound 0x101, the cell +0xB 0xC, +2 on.
S15_EXPORT void __cdecl ForetellOrb_Count(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0xF;
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) {
        MH_CALL(Sound_PlayById)(0x100);
        Dec(Sc()[2]);
        return;
    }
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[0xB] = 0xC;
    Inc(Sc()[2]);
}

// original 0x4B7B90: the cell drawn under the actor matrix; +9 down; at 0 the
// owner's +0xB down and the task freed.
S15_EXPORT void __cdecl ForetellOrb_End(void) {
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::ForetellOrb_Draw);
    MH_CALL(Gte_PopMatrix)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B7BD0: a 24 x 24 flat-textured quad (tpage 0 / 0x3C0 / 0, CLUT
// row 0x1E0, shade 0x80) centred on the screen point (+0x2E / +0x30), u from
// +0xB (12 wide), v 0x24..0x2E; between a draw-mode packet and layer 2.
S15_EXPORT void __cdecl ForetellOrb_Draw(void) {
    DrawModeCommit(0x15, 2);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    PutFloat(p + 8, S16(Sc() + 0x2E) - 0xC);
    PutFloat(p + 0xC, S16(Sc() + 0x30) - 0xC);
    PutFloat(p + 0x18, S16(Sc() + 0x2E) + 0xC);
    PutFloat(p + 0x1C, S16(Sc() + 0x30) - 0xC);
    PutFloat(p + 0x28, S16(Sc() + 0x2E) - 0xC);
    PutFloat(p + 0x2C, S16(Sc() + 0x30) + 0xC);
    PutFloat(p + 0x38, S16(Sc() + 0x2E) + 0xC);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    PutFloat(p + 0x3C, S16(Sc() + 0x30) + 0xC);
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 0, 0x3C0, 0);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1E0);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, clut);
    p[0x14] = Sc()[0xB];
    p[0x15] = 0x24;
    p[0x25] = 0x24;
    p[0x24] = static_cast<unsigned char>(Sc()[0xB] + 0xB);
    p[0x34] = Sc()[0xB];
    p[0x35] = 0x2E;
    p[0x45] = 0x2E;
    p[0x44] = static_cast<unsigned char>(Sc()[0xB] + 0xB);
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// original 0x4B7D40 (PSX Sacrifice 0x801EF700): pushes the matrix and loads
// Camera_Matrix x the actor's: translation RotTrans of (x >> 9 - 0x4000,
// z >> 9 - 0x4000, -(height / 2)) from Sprite_Current, no rotation. Pops
// nothing - its callers do. The MATRIX is one block (nine shorts, a pad, the
// translation at +0x14), as the original lays it out on its stack: RotTrans
// writes straight into its translation.
extern "C" void __cdecl MagicFx_PushActorMatrix(void) {
    g.push_matrix();
    const short rot[4] = {0, 0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    short v[4];
    v[0] = static_cast<short>((Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(sc + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16BattleItems(sc + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(offsetof(Matrix, t) == 0x14 && sizeof(Matrix) == 0x20, "MATRIX layout");
    long flag;
    g.rot_trans(v, m.t, &flag);
    g.rot_matrix(rot, m.m);
    g.mul_matrix0(Camera_Matrix, m.m, m.m);
    g.set_rot(reinterpret_cast<const unsigned long*>(&m));
    g.set_trans(reinterpret_cast<const unsigned long*>(&m));
}

void Foretell_Inject() {
    BOF3_INJECT(Foretell_Task);
    BOF3_INJECT(Foretell_Start);
    BOF3_INJECT(Foretell_Read);
    BOF3_INJECT(Foretell_Pause);
    BOF3_INJECT(Foretell_Show);
    BOF3_INJECT(Foretell_End);
    BOF3_INJECT(Foretell_DrawTiles);
    BOF3_INJECT(ForetellChild_Task);
    BOF3_INJECT(ForetellOrb_Run);
    BOF3_INJECT(ForetellOrb_Start);
    BOF3_INJECT(ForetellOrb_Spin);
    BOF3_INJECT(ForetellOrb_Count);
    BOF3_INJECT(ForetellOrb_End);
    BOF3_INJECT(ForetellOrb_Draw);
    BOF3_INJECT(MagicFx_PushActorMatrix);
}
