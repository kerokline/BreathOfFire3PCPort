// BOF3X_SHADOW=battle_sprites: the start-up differential fuzz of the 32
// functions in battle_sprites.cpp against byte-copies of the originals
// (docs/battle_sprites.md section 4).
//
// Each function is copied on its own with every call out re-aimed at a
// recording stand-in (bof3::CloneCall with `expected`), its jump table
// relocated into the copy; ours reach the same stand-ins through
// battle_sprites::g. The boss handler table 0x656954 is pointed at numbered
// stand-ins for the fuzz's duration. One round: one function, random bytes
// in every region it touches, then its branches' boundaries seeded (targets,
// counts, kinds, modes, the clut map's runs, the action ids the functions
// test for); theirs, then ours from the same state; the regions, the answer
// (at the width the original defines) and the stand-ins' log compared.
//
// The stand-ins answer what their callers test and disturb what their callers
// read again after the call (the target byte, the counts, the action id,
// record bytes, Sprite_Current and its bytes, the clut scratch) - and the
// clut searches leave a row and a cell in DamageScratch, which ClutMap_Mark
// reads after them.
//
// The inputs are bounded, not the functions: actor indices stay inside the
// records snapshotted (the originals index unchecked; a target of 11..0x3F
// would reach past .data), pop-up slots below 0x30, enemy data ids below
// 0x40, clut kinds whose divisor is not 0 for Sprite_SetClutStp.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_sprites.h"
#include "game/battle_sprites_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_sprites {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- our own buffers: the sprites, and what the swapped pointers point at ----

struct Mine {
    unsigned char sprites[4][0x100];
    unsigned char field[0x100];       // Field_State
    unsigned char action[0x10];       // *0x904B40
    unsigned char hit[0x20];          // *0x904B50
    unsigned char offsets[0x100];     // *0x939AD8
};
Mine g_mine;

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x6B43A9B5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Half() { return (Next() & 1) != 0; }
bool Often() { return Next() % 3 != 0; }
bool Rarely() { return Next() % 5 == 0; }
std::uint32_t High() { return Next() & 0xFFFFFF00u; }   // what a caller's register may hold above a byte
template <class T, std::size_t N> T Pick(const T (&values)[N]) { return values[Next() % N]; }

constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}

unsigned char* PoolSprite(unsigned k) {
    k %= 12;
    return k < 4 ? g_mine.sprites[k] : At(at::kEnemies + (k - 4) * at::kEnemyStride);
}

// Actor-ish values: an actor 0..10, or a side (0x40 and up).
unsigned char TargetValue() { return static_cast<unsigned char>(Often() ? Next() % 11 : 0x40 | Next()); }
std::uint16_t ActionId() {
    static const std::uint16_t kIds[] = {0x0E, 0x128, 0x4C, 0x4D, 0xB4, 0xB5, 0x4B, 0x4E, 0xB3, 0xB6, 0x0D, 0x0F, 0x127, 0x129};
    return static_cast<std::uint16_t>(Often() ? Pick(kIds) : Next() % 0x200);
}

void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    switch ((h >> 4) % 13) {
    case 0: At(at::kTarget)[0] = static_cast<unsigned char>(v % 11); break;   // read again as an index: an actor
    case 1: At(at::kActor)[0] = static_cast<unsigned char>(v % 11); break;
    case 2: SetWord(g_mine.action + 2, (h >> 16) % 3 ? ((h >> 16) & 1 ? 0x4C : 0xB5) : (h >> 16) % 0x200); break;
    case 3: {
        static const unsigned kOffsets[] = {0, 1, 0x90, 0xB7, 0x124, 0x125, 0x126, 0x127, 0x12C, 0x130, 0x131, 0x134, 0x148, 0xC8, 0xC9};
        At(at::kParty + ((h >> 20) % 3) * at::kPartyStride + kOffsets[(h >> 24) % 15])[0] = static_cast<unsigned char>(v);
        break;
    }
    case 4: {
        static const unsigned kOffsets[] = {0, 0x8C, 0xC7, 0x10C, 0x110, 0x101, 8, 1};
        unsigned char* const e = At(at::kEnemies + ((h >> 20) % 8) * at::kEnemyStride);
        const unsigned o = kOffsets[(h >> 24) % 8];
        e[o] = static_cast<unsigned char>(o == 0x8C ? v % 6 : v);
        break;
    }
    case 5:
        switch ((h >> 20) % 5) {
        case 0: At(at::kPartyCount)[0] = static_cast<unsigned char>(v % 5); break;
        case 1: At(at::kEnemyCount)[0] = static_cast<unsigned char>(v % 9); break;
        case 2: At(at::kPartyPick)[0] = static_cast<unsigned char>(v % 3); break;
        case 3: At(at::kEnemyPick)[0] = static_cast<unsigned char>(v % 3); break;
        default: At(at::kBoss)[0] = static_cast<unsigned char>(v % 24); break;
        }
        break;
    case 6: Sprite_Current = PoolSprite(v); break;
    case 11:   // the members' chosen targets, re-read by Battle_MemberAutoTarget: enemies
        for (unsigned i = 0; i < 3; ++i) At(at::kParty + i * at::kPartyStride)[0x124] = static_cast<unsigned char>(3 + (v + i) % 8);
        break;
    case 7: SetWord(At(0x802E08 + ((h >> 20) % 3) * at::kPartyStride), h >> 16); break;
    case 8: At(at::kCells)[(h >> 20) & 1] = static_cast<unsigned char>((h >> 20) & 1 ? v % 16 : v % 6); break;
    case 9: At(at::kClutMap)[v % 64] = static_cast<unsigned char>((h >> 20) % 3 ? 0xFF : v); break;
    case 10: {
        unsigned char* const s = Sprite_Current;
        switch ((h >> 20) % 4) {
        case 0: s[5] = static_cast<unsigned char>(v % 11); break;
        case 1: s[0x27] = static_cast<unsigned char>(v); break;
        case 2: s[0x28] = static_cast<unsigned char>(v % 7); break;
        default: s[0x24] = static_cast<unsigned char>(v); break;
        }
        break;
    }
    default: break;
    }
}

// --- the stand-ins ---------------------------------------------------------

enum Stub : unsigned {
    kActorOut, kRand, kMark, kInvPut, kItemClass, kPopupSlot, kAddClamped, kAddCap, kWindow, kPlayEffect, kElevation,
    kSetBank, kUpdateScreen, kChanceB, kActFlag, kSlotsFull, kCoinFlip, kAutoAllowed, kAutoFixed, kNoTarget, kOutAction,
    kActionE, kPickParty, kPickEnemyA, kPickEnemyB, kClutMark, kClutFind, kClutOwner, kBossCommon, kBoss, kNormal,
    kSetupEnemy, kCopyEnemy, kEnemyOffset, kKindFlag, kStubs
};
struct StubSpec { std::uint32_t target; std::uint32_t mask[4]; };
const StubSpec kSpecs[kStubs] = {
    {0x4456C0, {0xFF}}, {0x5B93D2, {}}, {0x446FB0, {0xFF}}, {0x590C90, {0xFF, 0xFF, 0xFF, 0xFF}},
    {0x591810, {0xFF, 0xFF}}, {0x435180, {0xFF, 0xFF}}, {0x590E30, {~0u, 0xFFFF}}, {0x590F30, {~0u, 0xFF}},
    {0x59E2D0, {0xFF, 0xFF}}, {0x587740, {0xFFFF}}, {0x5720C0, {~0u, ~0u}}, {0x589590, {0xFFFF}}, {0x588F20, {}},
    {0x453910, {0xFF}}, {0x453A90, {0xFF}}, {0x453AC0, {0xFF}}, {0x453A10, {0xFF}}, {0x452DD0, {0xFF}},
    {0x454290, {0xFF}}, {0x446B00, {}}, {0x454220, {0xFF}}, {0x454260, {0xFF}}, {0x454310, {0xFF}},
    {0x435C80, {0xFF}}, {0x445730, {0xFF}}, {0x454DF0, {0xFF, 0xFF, 0xFF}}, {0x454F30, {0xFF}}, {0x455140, {0xFF}},
    {0x494500, {}}, {0x4942A0, {}}, {0x4942C0, {}}, {0x494320, {0xFF, 0xFFFF, ~0u, ~0u}}, {0x4946C0, {0xFF, 0xFF}},
    {0x494F00, {0xFF, 0xFFFF}}, {0x494EA0, {0xFFFF}},
};

std::uint32_t Handle(unsigned id, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    const StubSpec& s = kSpecs[id];
    std::uint32_t extra = 0;
    if (id == kSetBank || id == kUpdateScreen || id == kEnemyOffset || id == kElevation) extra = Address(Sprite_Current);
    Record(id + 1, a & s.mask[0], b & s.mask[1], c & s.mask[2], extra ? extra : d & s.mask[3]);
    std::uint32_t h = Hash();
    switch (id) {
    case kRand: {
        // Multiples of 12 and of 11 half the time, so that a remainder test
        // against a neighbouring constant tells; else anything 0..0x7FFF.
        Disturb();
        const std::uint32_t r = Hash();
        switch (r % 4) {
        case 0: return ((r >> 8) % 2000) * 12;
        case 1: return ((r >> 8) % 2000) * 11;
        default: return (r >> 8) & 0x7FFF;
        }
    }
    case kPopupSlot: Disturb(); return (h >> 8) % 0x30;
    case kClutFind:
    case kClutOwner:
        if (h % 4) {
            At(at::kCells)[0] = static_cast<unsigned char>((h >> 8) % 6);
            At(at::kCells)[1] = static_cast<unsigned char>((h >> 12) % 16);
        }
        break;
    case kWindow: {
        unsigned char* const w = At(at::kWindows + (a & 0xFF) * at::kWindowStride);
        w[0] = 1;
        w[1] = static_cast<unsigned char>(b);
        w[2] = static_cast<unsigned char>(h >> 8);
        w[3] = static_cast<unsigned char>(h >> 16);
        w[8] = static_cast<unsigned char>(h >> 20);
        break;
    }
    case kAddClamped:
        if (h % 2) SetWord(At(a), h >> 12);
        break;
    case kItemClass:   // Battle_MemberAutoTarget reads the member's +0x124 again after it
        if (h % 2)
            for (unsigned i = 0; i < 3; ++i) At(at::kParty + i * at::kPartyStride)[0x124] = static_cast<unsigned char>(3 + (h >> 8) % 8);
        break;
    case kAddCap:
        if (h % 2) At(a)[0] = static_cast<unsigned char>(h >> 12);
        break;
    default: break;
    }
    Disturb();
    h = Hash();
    // Answers: nought, one, or any byte; a whole dword for the rest.
    switch (h % 4) {
    case 0: case 1: return 0;
    case 2: return 1;
    default: return h >> 4;
    }
}

template <unsigned N> std::uint32_t __cdecl StubFn(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    return Handle(N, a, b, c, d);
}
template <unsigned N> void __cdecl BossFn() { Record(0x100 + N); Disturb(); }

using Raw = const void*;
template <unsigned... N> struct Table {
    static inline const Raw kStubs[] = {reinterpret_cast<Raw>(&StubFn<N>)...};
    static inline const Raw kBosses[] = {reinterpret_cast<Raw>(&BossFn<N>)...};
};
using Stubs = Table<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                    30, 31, 32, 33, 34>;
constexpr unsigned kBossHandlers = 24;

Raw StubFor(std::uint32_t target) {
    for (unsigned i = 0; i < kStubs; ++i)
        if (kSpecs[i].target == target) return Stubs::kStubs[i];
    bof3::Fatal("battle_sprites: no stand-in for a call to 0x%X", (unsigned)target);
}

template <class F> F As(unsigned i) { return reinterpret_cast<F>(const_cast<void*>(Stubs::kStubs[i])); }
Callees StubCallees() {
    Callees c = kOriginals;
    c.actor_out = As<decltype(c.actor_out)>(kActorOut);
    c.rand = As<decltype(c.rand)>(kRand);
    c.mark_actor = As<decltype(c.mark_actor)>(kMark);
    c.inventory_put = As<decltype(c.inventory_put)>(kInvPut);
    c.item_class = As<decltype(c.item_class)>(kItemClass);
    c.popup_slot = As<decltype(c.popup_slot)>(kPopupSlot);
    c.add_clamped = As<decltype(c.add_clamped)>(kAddClamped);
    c.add_cap100 = As<decltype(c.add_cap100)>(kAddCap);
    c.window_open = As<decltype(c.window_open)>(kWindow);
    c.play_effect = As<decltype(c.play_effect)>(kPlayEffect);
    c.elevation = As<decltype(c.elevation)>(kElevation);
    c.set_bank = As<decltype(c.set_bank)>(kSetBank);
    c.update_screen = As<decltype(c.update_screen)>(kUpdateScreen);
    c.member_chance_b = As<decltype(c.member_chance_b)>(kChanceB);
    c.action_flag = As<decltype(c.action_flag)>(kActFlag);
    c.slots_full = As<decltype(c.slots_full)>(kSlotsFull);
    c.member_chance = As<decltype(c.member_chance)>(kCoinFlip);
    c.auto_allowed = As<decltype(c.auto_allowed)>(kAutoAllowed);
    c.auto_fixed = As<decltype(c.auto_fixed)>(kAutoFixed);
    c.no_target = As<decltype(c.no_target)>(kNoTarget);
    c.out_action = As<decltype(c.out_action)>(kOutAction);
    c.action_is_e = As<decltype(c.action_is_e)>(kActionE);
    c.pick_party = As<decltype(c.pick_party)>(kPickParty);
    c.pick_enemy_a = As<decltype(c.pick_enemy_a)>(kPickEnemyA);
    c.pick_enemy_b = As<decltype(c.pick_enemy_b)>(kPickEnemyB);
    c.clut_mark = As<decltype(c.clut_mark)>(kClutMark);
    c.clut_find = As<decltype(c.clut_find)>(kClutFind);
    c.clut_owner = As<decltype(c.clut_owner)>(kClutOwner);
    c.boss_common = As<decltype(c.boss_common)>(kBossCommon);
    c.boss_encounter = As<decltype(c.boss_encounter)>(kBoss);
    c.normal_encounter = As<decltype(c.normal_encounter)>(kNormal);
    c.setup_enemy = As<decltype(c.setup_enemy)>(kSetupEnemy);
    c.copy_enemy = As<decltype(c.copy_enemy)>(kCopyEnemy);
    c.enemy_offset = As<decltype(c.enemy_offset)>(kEnemyOffset);
    c.kind_flag = As<decltype(c.kind_flag)>(kKindFlag);
    return c;
}

// --- the copies ---------------------------------------------------------------
// Extents and calls out listed by capstone 2026-09-23 (a scratch script that
// fails on any transfer leaving the extent other than a direct call or tail
// jump): offsets are of the E8 / E9 byte. Jump tables: {operand, table,
// entries} as offsets into the function.

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[20];
    int n_calls;
    move_script::Table table;
    std::uint32_t result_mask;
    unsigned regions;
};

enum Region : unsigned {
    kRParty = 1, kRGlobals = 2, kRBattle = 4, kRTints = 8, kRScratch = 16, kRCurrent = 32, kRData = 64, kREncounter = 128,
    kRActions = 256, kRStrip = 512, kRMine = 1024,
};
constexpr unsigned kBase = kRParty | kRGlobals | kRBattle | kRTints | kRScratch | kRCurrent | kRMine;
constexpr move_script::Table kNoTable = {0, 0, 0};

enum Fn : unsigned {
    fRoll, fSetFlags, fFlag40, fAnyF0, fActionSuits, fItemSuits, fPick8, fCoinFlip, fSettle8, fFormation, fDamagePopup,
    fAutoTarget, fOutAction, fHitSound, fHitPopup, fSetTint, fTintRelease, fClutMark, fClutFind, fClutOwner, fClutStp,
    fEncounterKind, fBossEncounter, fInitEnemies, fSetupEnemy, fCopyEnemy, fEnemyNames, fKindFlag, fSetKindFlag,
    fEnemyOffset, fScreenSlot, fPlayById, kFunctions
};

const Clone kClones[kFunctions] = {
    {"Battle_RollPendingFlag", 0x452BF0, 0x1D5,
     {{0x46, 0x4456C0}, {0x58, 0x5B93D2}, {0x9D, 0x4456C0}, {0xAF, 0x5B93D2}, {0xFB, 0x4456C0}, {0x12C, 0x5B93D2},
      {0x16B, 0x4456C0}, {0x194, 0x5B93D2}}, 8, kNoTable, 0xFF, kBase},
    {"Battle_SetTargetFlags", 0x452F70, 0x151,
     {{0x2A, 0x4456C0}, {0x4B, 0x446FB0}, {0x81, 0x4456C0}, {0xA3, 0x446FB0}, {0xC2, 0x4456C0}, {0x106, 0x446FB0},
      {0x144, 0x446FB0}}, 7, kNoTable, 0, kBase},
    {"Battle_SetTargetFlag40", 0x4530D0, 0xB9, {{0x1F, 0x4456C0}, {0x5A, 0x4456C0}}, 2, kNoTable, 0, kBase},
    {"Battle_AnyFlagF0", 0x453190, 0x7B, {}, 0, kNoTable, 0xFF, kBase},
    {"Battle_ActionSuitsTarget", 0x453210, 0x87, {{0x29, 0x4456C0}}, 1, kNoTable, 0xFF, kBase | kRActions},
    {"Battle_ItemSuitsTarget", 0x4532A0, 0x5B, {{0x10, 0x591810}, {0x27, 0x4456C0}}, 2, kNoTable, 0xFF, kBase},
    {"Battle_PickFlag8Member", 0x4537A0, 0x16C,
     {{0x48, 0x453910}, {0x54, 0x453A10}, {0x96, 0x453910}, {0x9D, 0x453A10}, {0xD1, 0x453910}, {0xDD, 0x453A10},
      {0x120, 0x5B93D2}}, 7, kNoTable, 0xFF, kBase | kRActions},
    {"Battle_MemberCoinFlip", 0x453A10, 0x73, {{0x6, 0x4456C0}, {0x4B, 0x453AC0}, {0x5B, 0x5B93D2}}, 3, kNoTable, 0xFF, kBase},
    {"Battle_SettleFlag8", 0x453B10, 0xE1, {{0x1B, 0x4456C0}, {0x3E, 0x453A90}, {0x69, 0x590C90}, {0x80, 0x590C90}}, 4,
     kNoTable, 0xFF, kBase | kRActions},
    {"Formation_ApplyStatMods", 0x453C00, 0x19C,
     {{0x2E, 0x590E30}, {0x46, 0x590E30}, {0x56, 0x590F30}, {0x7A, 0x4456C0}, {0xAE, 0x590E30}, {0xE1, 0x4456C0},
      {0x123, 0x590E30}, {0x14E, 0x590E30}, {0x165, 0x590E30}, {0x17D, 0x590E30}}, 10, {0x1A, 0x18C, 4}, 0, kBase},
    {"Battle_SetDamagePopup", 0x453DA0, 0x103, {{0x7, 0x435180}}, 1, kNoTable, 0, kBase},
    {"Battle_MemberAutoTarget", 0x453FA0, 0x27C,
     {{0x43, 0x452DD0}, {0x68, 0x454290}, {0x7D, 0x446B00}, {0xBC, 0x4456C0}, {0xC8, 0x446B00}, {0xDC, 0x4456C0},
      {0xE9, 0x454220}, {0x129, 0x446B00}, {0x13B, 0x454310}, {0x15B, 0x4456C0}, {0x16C, 0x454260}, {0x19B, 0x591810},
      {0x1A7, 0x446B00}, {0x1C3, 0x454310}, {0x1EC, 0x435C80}, {0x1FE, 0x445730}, {0x21F, 0x5B93D2}, {0x234, 0x454310},
      {0x23B, 0x5B93D2}, {0x245, 0x445730}}, 20, {0xAD, 0x264, 6}, 0, kBase | kRActions},
    {"Battle_MemberOutAction", 0x454220, 0x3D, {}, 0, kNoTable, 0xFF, kBase},
    {"Battle_PlayHitSound", 0x454380, 0x83, {{0x79, 0x587740}}, 1, kNoTable, 0, kBase},
    {"Battle_SetHitPopup", 0x454410, 0x17E, {{0x7, 0x435180}}, 1, kNoTable, 0, kBase},
    {"Sprite_SetTint", 0x454CC0, 0x9C, {{0x52, 0x454DF0}}, 1, kNoTable, 0xFF, kBase},
    {"Tint_Release", 0x454D60, 0x56, {{0x4C, 0x454DF0}}, 1, kNoTable, 0, kBase},
    {"ClutMap_Mark", 0x454DF0, 0x140, {{0x10, 0x454F30}, {0x23, 0x455140}}, 2, {0x5E, 0x12C, 5}, 0xFF, kBase},
    {"ClutMap_FindFree", 0x454F30, 0x210, {}, 0, {0x55, 0x1FC, 5}, 0xFF, kBase},
    {"ClutMap_FindOwner", 0x455140, 0x5F, {}, 0, kNoTable, 0xFF, kBase},
    {"Sprite_SetClutStp", 0x4551A0, 0xA6, {}, 0, kNoTable, 0, kBase | kRStrip},
    {"Battle_InitEncounterKind", 0x494280, 0x14, {{0x9, 0x4942A0}, {0xE, 0x4942C0}}, 2, kNoTable, 0, kBase},
    {"Battle_InitBossEncounter", 0x4942A0, 0x13, {{0x0, 0x494500}}, 1, kNoTable, 0, kBase},
    {"Battle_InitEnemies", 0x4942C0, 0x5A, {{0x32, 0x494320}}, 1, kNoTable, 0, kBase | kREncounter},
    {"Battle_SetupEnemy", 0x494320, 0x1D9,
     {{0x16C, 0x5720C0}, {0x18C, 0x589590}, {0x1A5, 0x4946C0}, {0x1AC, 0x494F00}, {0x1BA, 0x494EA0}}, 5, kNoTable, 0,
     kBase | kRData},
    {"Battle_CopyEnemyData", 0x4946C0, 0x21C, {}, 0, kNoTable, 0, kBase | kRData},
    {"Battle_OpenEnemyNames", 0x494A80, 0x3EE, {{0x1B2, 0x59E2D0}, {0x32E, 0x59E2D0}}, 2, kNoTable, 0, kBase},
    {"Battle_EnemyKindFlag", 0x494EA0, 0x2D, {}, 0, kNoTable, 0xFF, kBase},
    {"Battle_SetEnemyKindFlag", 0x494ED0, 0x2D, {}, 0, kNoTable, 0, kBase},
    {"Battle_SetEnemyOffset", 0x494F00, 0x13C, {}, 0, {0x17, 0x12C, 4}, 0, kBase | kRData},
    {"Sprite_UpdateScreenSlot", 0x588F00, 0x13, {{0xE, 0x588F20}}, 1, kNoTable, 0, kBase},
    {"Sound_PlayById", 0x587900, 0xC, {{0x5, 0x587740}}, 1, kNoTable, 0, kBase},
};
const Raw kOurs[kFunctions] = {
    reinterpret_cast<Raw>(&Battle_RollPendingFlag), reinterpret_cast<Raw>(&Battle_SetTargetFlags),
    reinterpret_cast<Raw>(&Battle_SetTargetFlag40), reinterpret_cast<Raw>(&Battle_AnyFlagF0),
    reinterpret_cast<Raw>(&Battle_ActionSuitsTarget), reinterpret_cast<Raw>(&Battle_ItemSuitsTarget),
    reinterpret_cast<Raw>(&Battle_PickFlag8Member), reinterpret_cast<Raw>(&Battle_MemberCoinFlip),
    reinterpret_cast<Raw>(&Battle_SettleFlag8), reinterpret_cast<Raw>(&Formation_ApplyStatMods),
    reinterpret_cast<Raw>(&Battle_SetDamagePopup), reinterpret_cast<Raw>(&Battle_MemberAutoTarget),
    reinterpret_cast<Raw>(&Battle_MemberOutAction), reinterpret_cast<Raw>(&Battle_PlayHitSound),
    reinterpret_cast<Raw>(&Battle_SetHitPopup), reinterpret_cast<Raw>(&Sprite_SetTint), reinterpret_cast<Raw>(&Tint_Release),
    reinterpret_cast<Raw>(&ClutMap_Mark), reinterpret_cast<Raw>(&ClutMap_FindFree), reinterpret_cast<Raw>(&ClutMap_FindOwner),
    reinterpret_cast<Raw>(&Sprite_SetClutStp), reinterpret_cast<Raw>(&Battle_InitEncounterKind),
    reinterpret_cast<Raw>(&Battle_InitBossEncounter), reinterpret_cast<Raw>(&Battle_InitEnemies),
    reinterpret_cast<Raw>(&Battle_SetupEnemy), reinterpret_cast<Raw>(&Battle_CopyEnemyData),
    reinterpret_cast<Raw>(&Battle_OpenEnemyNames), reinterpret_cast<Raw>(&Battle_EnemyKindFlag),
    reinterpret_cast<Raw>(&Battle_SetEnemyKindFlag), reinterpret_cast<Raw>(&Battle_SetEnemyOffset),
    reinterpret_cast<Raw>(&Sprite_UpdateScreenSlot), reinterpret_cast<Raw>(&Sound_PlayById),
};

using FnPtr = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

// --- the state ------------------------------------------------------------------

struct Span { std::uint32_t at, size; unsigned region; };
const Span kSpans[] = {
    {at::kParty, 0xFC0, kRParty},                 // three party records, over-indexed ones, the window records
    {0x904000, 0xC00, kRGlobals},                 // the battle globals and the two bit sets
    {at::kPopups, 0x3600, kRBattle},              // the pop-ups and the enemy records
    {0x7E06A0, 0xD60, kRTints},                   // the clut map and the tint records (and 64 records past them)
    {at::kCells, 8, kRScratch},
    {Address(&Sprite_Current), 0xC, kRCurrent},   // Sprite_Current .. Gfx_ClutStripDirty
    {0x8C5580, 0x2680, kRData},                   // the enemy data, ids below 0x40
    {at::kEncounter, 0x60, kREncounter},
    {at::kActions, 0x3000, kRActions},            // action ids below 0x200
    {Address(Gfx_ClutStrip), 0x20400, kRStrip},
    {Address(&g_mine), sizeof(Mine), kRMine},
};
constexpr std::size_t kStateBytes = 0xFC0 + 0xC00 + 0x3600 + 0xD60 + 8 + 0xC + 0x2680 + 0x60 + 0x3000 + 0x20400 + sizeof(Mine);

struct State {
    unsigned char memory[kStateBytes];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s, unsigned regions) {
    std::size_t at = 0;
    for (const Span& r : kSpans) {
        if (regions & r.region) std::memcpy(s.memory + at, At(r.at), r.size);
        at += r.size;
    }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s, unsigned regions) {
    std::size_t at = 0;
    for (const Span& r : kSpans) {
        if (regions & r.region) std::memcpy(At(r.at), s.memory + at, r.size);
        at += r.size;
    }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
bool Same(const State& x, const State& y, unsigned regions, std::size_t& first) {
    std::size_t at = 0;
    for (const Span& r : kSpans) {
        if ((regions & r.region) && std::memcmp(x.memory + at, y.memory + at, r.size) != 0) {
            first = at;
            while (x.memory[first] == y.memory[first]) ++first;
            return false;
        }
        at += r.size;
    }
    first = kStateBytes;
    return x.result == y.result && x.log_n == y.log_n && std::memcmp(x.log, y.log, sizeof x.log) == 0;
}

// --- the seeding ------------------------------------------------------------------

void Fill(std::uint32_t address, std::uint32_t size) {
    unsigned char* const p = At(address);
    for (std::uint32_t i = 0; i + 4 <= size; i += 4) SetLong(p + i, static_cast<std::int32_t>(Next()));
}

void SeedSprite(unsigned char* s) {
    s[5] = static_cast<unsigned char>(Next() % 11);
    s[0x28] = static_cast<unsigned char>(Often() ? Next() % 5 : Next() % 7);
    s[0x24] = static_cast<unsigned char>(Next());
}

void SeedClutMap() {
    unsigned char* const map = At(at::kClutMap);
    for (unsigned row = 0; row < 4; ++row) {
        const unsigned style = Next() % 5;
        for (unsigned c = 0; c < 16; ++c) {
            unsigned char v;
            switch (style) {
            case 0: v = 0xFF; break;                                             // a whole free row
            case 1: v = Next() % 5 ? 0xFF : static_cast<unsigned char>(Next() % 32); break;
            case 2: v = static_cast<unsigned char>((c / 4 + row) % 3 ? 0xFF : Next() % 32); break;   // aligned holes
            case 4: v = static_cast<unsigned char>(c == (row & 1 ? 15 : 0) ? Next() % 32 : 0xFF); break;   // one cell short of a row
            default: v = static_cast<unsigned char>(Half() ? 0xFF : Next() % 32); break;
            }
            map[row * 16 + c] = v;
        }
    }
    if (Rarely()) std::memset(map, Next() % 32, 64);      // nothing free at all
}

void SeedRound(unsigned regions) {
    for (const Span& r : kSpans)
        if ((regions & r.region) && r.region != kRStrip && r.region != kRCurrent) Fill(r.at, r.size);
    // pointers and the fields the functions branch on
    SetLong(At(at::kAction), static_cast<std::int32_t>(Address(g_mine.action)));
    SetLong(At(at::kHitBlock), static_cast<std::int32_t>(Address(g_mine.hit)));
    SetWord(g_mine.action + 2, ActionId());
    At(at::kTarget)[0] = TargetValue();
    At(at::kActor)[0] = static_cast<unsigned char>(Next() % 11);
    At(at::kPartyCount)[0] = static_cast<unsigned char>(Next() % 5);
    At(at::kPartyPick)[0] = static_cast<unsigned char>(Next() % 3);
    At(at::kEnemyCount)[0] = static_cast<unsigned char>(Often() ? Next() % 9 : 8);
    At(at::kEnemyPick)[0] = static_cast<unsigned char>(Next() % 3);
    At(at::kBoss)[0] = static_cast<unsigned char>(Half() ? 0 : Next() % kBossHandlers);
    At(at::kLayout)[0] = static_cast<unsigned char>(Often() ? Next() % 4 : Next());
    At(at::kFormation)[0] = static_cast<unsigned char>(Next() % 6);
    static const unsigned char kKinds[] = {0x1B, 0x3F, 0x44, 0x1A, 0x1C, 0x40, 0x43, 0x45};
    g_mine.field[0x92] = Often() ? Pick(kKinds) : static_cast<unsigned char>(Next());
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const p = At(at::kParty + i * at::kPartyStride);
        static const unsigned char kClasses[] = {0, 5, 6, 7, 0xFF};
        p[0xB7] = Half() ? Pick(kClasses) : static_cast<unsigned char>(Next());
        p[0x124] = TargetValue();
        p[0x125] = static_cast<unsigned char>(Often() ? Next() % 7 : Next());
        SetWord(p + 0x126, ActionId());
        std::uint32_t status = static_cast<std::uint32_t>(Long(p + 0x134)) & ~0x14001u;
        if (Rarely()) status |= 1;
        if (Rarely()) status |= Half() ? 0x4000 : 0x10000;
        SetLong(p + 0x134, static_cast<std::int32_t>(status));
        if (Half()) p[0x90] &= 0xDF;
    }
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const e = At(at::kEnemies + k * at::kEnemyStride);
        SeedSprite(e);
        e[0x8C] = static_cast<unsigned char>(Often() ? Next() % 4 : Next() % 8);
        static const unsigned char kClasses[] = {0, 5, 6, 7, 0xFF};
        e[0xC7] = Half() ? Pick(kClasses) : static_cast<unsigned char>(Next());
    }
    for (auto& s : g_mine.sprites) SeedSprite(s);
    // the tint records' sprites, and 64 records' worth past the 32
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = At(at::kTints + i * 12);
        SetLong(rec + 8, static_cast<std::int32_t>(Address(PoolSprite(Next()))));
        if (Half()) rec[0] &= 0xFE;
        rec[5] = static_cast<unsigned char>(Next() % 7);
    }
    if (Rarely())
        for (unsigned i = 0; i < 32; ++i) At(at::kTints + i * 12)[0] |= 1;   // every record taken
    SeedClutMap();
    At(at::kCells)[0] = static_cast<unsigned char>(Next() % 6);
    At(at::kCells)[1] = static_cast<unsigned char>(Next() % 16);
    Sprite_Current = PoolSprite(Next());
    if (regions & kREncounter)
        for (unsigned r = 0; r < 8; ++r) At(at::kEncounter + r * 12)[0] = static_cast<unsigned char>(Half() ? 0 : Next());
}

// A round of Battle_OpenEnemyNames with eight living enemies of eight kinds:
// the walk runs past the list (docs/battle_sprites.md section 1).
void SeedEightKinds() {
    At(at::kEnemyCount)[0] = 8;
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const e = At(at::kEnemies + k * at::kEnemyStride);
        e[0] |= 1;
        e[0x8C] = static_cast<unsigned char>(0x10 + k);
    }
}

// The arguments of function f, as whole dwords (the originals' callers push
// registers whose upper bytes are whatever they held).
void Arguments(unsigned f, std::uint32_t (&a)[4]) {
    for (auto& x : a) x = Next();
    switch (f) {
    case fSetFlags: a[0] = High() | TargetValue(); break;
    case fFlag40: a[0] = High() | TargetValue(); break;
    case fCoinFlip: case fAutoTarget: case fOutAction: a[0] = High() | (Often() ? Next() % 3 : Next() % 11); break;
    case fDamagePopup: {
        static const std::uint32_t kAmounts[] = {0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x8001, 2, 0xFFFE};
        a[0] = (Next() & 0xFFFF0000u) | (Often() ? Pick(kAmounts) : Next() & 0xFFFF);
        a[1] = High() | (Next() % 11);
        break;
    }
    case fSetTint: a[0] = Address(Sprite_Current); break;
    case fTintRelease: a[0] = High() | (Often() ? Next() % 32 : Next() % 64); break;
    case fClutMark: a[0] = High() | (Next() % 7); a[2] = Half() ? High() : Next(); break;
    case fClutFind: a[0] = High() | (Often() ? Next() % 5 : Next() % 8); break;
    case fClutOwner: a[0] = High() | (Often() ? At(at::kClutMap)[Next() % 64] : Next() % 32); break;
    case fSetupEnemy: a[0] = High() | (Next() % 8); a[1] = (Next() & 0xFFFF0000u) | (Next() % 0x40); break;
    case fCopyEnemy: a[0] = High() | (Next() % 8); a[1] = (Next() & 0xFFFFFF00u) | (Next() % 0x40); break;   // the id by its low byte
    case fEnemyOffset: a[0] = High() | (Next() % 8); a[1] = (Next() & 0xFFFF0000u) | (Next() % 0x40); break;  // by 16 bits
    case fKindFlag: case fSetKindFlag: a[0] = (Next() & 0xFFFF0000u) | (Next() % 0x4000); break;
    case fEnemyNames: a[0] = 0xFFFF; break;   // the stack word above the return address: see SeedEightKinds
    default: break;
    }
    if (f == fClutStp) {
        unsigned char* const s = Sprite_Current;
        s[0x28] = static_cast<unsigned char>(Next() % 5);
        static const unsigned char kSlots[] = {0xC0, 0xCF, 0xFF, 0x1C, 0x1F, 0xE0, 0xFF, 0x70, 0x7F, 0x38, 0x3F, 0};
        s[0x27] = Often() ? Pick(kSlots) : static_cast<unsigned char>(Next());
    }
    if (f == fSetupEnemy && Half()) {
        static const std::uint16_t kBanks[] = {0x2CD, 0x2E1, 0x2CC, 0x2CE, 0x2E0, 0x2E2};
        SetWord(At(at::kEnemyData + (a[1] & 0xFFFF) * at::kEnemyDataStride + 0x1A), Pick(kBanks));
    }
    if (f == fCoinFlip && Often()) {
        unsigned char* const p = At(at::kParty + (a[0] & 0xFF) * at::kPartyStride);
        p[0x124] = At(at::kActor)[0];
        p[0x130] |= 1;
    }
    if (f == fAutoTarget && Often()) {   // the plain path to modes 4 and 5
        unsigned char* const p = At(at::kParty + (a[0] & 0xFF) * at::kPartyStride);
        p[0x90] &= 0xDF;
        SetLong(p + 0x134, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p + 0x134)) & ~0x14001u));
        At(at::kBattleFlags)[0] &= 0xEF;
        p[0x124] = static_cast<unsigned char>(Next() % 3);
        p[0x125] = static_cast<unsigned char>(4 + Next() % 2);
    }
    if (f == fEnemyOffset && Often()) Sprite_Current[8] = static_cast<unsigned char>(Next() % 4);
    if (f == fHitPopup && Half()) Sprite_Current[5] = static_cast<unsigned char>(Next() % 3);
    if (f == fEnemyNames && Rarely()) SeedEightKinds();
    if (f == fAnyF0)
        for (unsigned i = 0; i < 11; ++i) {
            unsigned char* const r = i < 3 ? At(at::kParty + i * at::kPartyStride) : At(at::kEnemies + (i - 3) * at::kEnemyStride);
            if (Often()) r[i < 3 ? 0x130 : 0x110] &= 0x0F;
        }
    if (f == fAnyF0 && Half()) {   // one present member with bit 7 alone
        unsigned char* const p = At(at::kParty + (Next() % 3) * at::kPartyStride);
        p[0] |= 1;
        p[0x130] = 0x80;
    }
}

// --- the rounds ---------------------------------------------------------------

struct Result { unsigned bad, calls, paths; };

unsigned PathHash(const State& s) {
    std::uint32_t h = s.result * 0x9E3779B1u + s.log_n;
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i) h = (h ^ s.log[i].what) * 0x01000193u;
    return h;
}

Result RunOne(unsigned f, void* theirs, unsigned rounds) {
    static State input, their_out, our_out;
    static std::uint32_t seen[512];
    unsigned n_seen = 0;
    Result r = {0, 0, 0};
    const Clone& c = kClones[f];
    for (unsigned round = 0; round < rounds; ++round) {
        SeedRound(c.regions);
        g_seed = Next();
        std::uint32_t a[4];
        Arguments(f, a);
        std::memset(g_log, 0, sizeof g_log);
        g_log_n = 0;
        Capture(input, c.regions);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, c.regions);
            const FnPtr fn = reinterpret_cast<FnPtr>(const_cast<void*>(pass ? kOurs[f] : theirs));
            const std::uint32_t result = fn(a[0], a[1], a[2], a[3]) & c.result_mask;
            State& out = pass ? our_out : their_out;
            Capture(out, c.regions);
            out.result = result;
        }
        r.calls += their_out.log_n;
        const unsigned path = PathHash(their_out);
        bool known = false;
        for (unsigned i = 0; i < n_seen && !known; ++i) known = seen[i] == path;
        if (!known && n_seen < 512) seen[n_seen++] = path;
        std::size_t first = 0;
        if (!Same(their_out, our_out, c.regions, first) && ++r.bad <= 4)
            bof3::Log("shadow      battle_sprites self-test MISMATCH: %s round %u (0x%X, 0x%X, 0x%X, 0x%X): result 0x%X / 0x%X, "
                      "log %u / %u, first state difference at byte 0x%X",
                      c.name, round, a[0], a[1], a[2], a[3], their_out.result, our_out.result, their_out.log_n,
                      our_out.log_n, (unsigned)first);
        if (r.bad <= 4)
            for (unsigned i = 0; i < kLog && i < their_out.log_n; ++i)
                if (std::memcmp(&their_out.log[i], &our_out.log[i], sizeof(Entry)) != 0) {
                    const Entry& x = their_out.log[i];
                    const Entry& y = our_out.log[i];
                    bof3::Log("shadow      battle_sprites   log entry %u: theirs %u (0x%X 0x%X 0x%X 0x%X), ours %u (0x%X 0x%X 0x%X 0x%X)", i,
                              x.what, x.a, x.b, x.c, x.d, y.what, y.a, y.b, y.c, y.d);
                    break;
                }
    }
    r.paths = n_seen;
    return r;
}

}  // namespace

void SelfTest() {
    std::size_t bytes = 0;
    for (const Span& s : kSpans) bytes += s.size;
    if (bytes != kStateBytes) bof3::Fatal("battle_sprites: the spans are %u bytes, the state holds %u", (unsigned)bytes, (unsigned)kStateBytes);

    void* theirs[kFunctions];
    for (unsigned f = 0; f < kFunctions; ++f) {
        const Clone& c = kClones[f];
        bof3::CloneCall calls[20];
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        theirs[f] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (!theirs[f]) bof3::Fatal("battle_sprites: could not copy %s", c.name);
        if (c.table.entries) move_script::Relocate(theirs[f], c.base, c.size, c.table);
    }

    // What the fuzz swaps in, and what it puts back.
    static State saved;
    constexpr unsigned kAll = 0x7FF;
    Capture(saved, kAll);
    unsigned char* const saved_current = Sprite_Current;
    unsigned char* const saved_field = Field_State;
    const std::int32_t saved_offsets = Long(At(at::kEnemyOffsets));
    std::uint32_t saved_bosses[kBossHandlers];
    std::memcpy(saved_bosses, At(at::kBossHandlers), sizeof saved_bosses);
    Field_State = g_mine.field;
    SetLong(At(at::kEnemyOffsets), static_cast<std::int32_t>(Address(g_mine.offsets)));
    for (unsigned i = 0; i < kBossHandlers; ++i)
        SetLong(At(at::kBossHandlers + i * 4), static_cast<std::int32_t>(Address(Stubs::kBosses[i % 35])));

    g = StubCallees();
    unsigned bad = 0, rounds_total = 0, calls = 0;
    char line[900];
    int used = 0;
    for (unsigned f = 0; f < kFunctions; ++f) {
        const unsigned rounds = f == fClutStp ? 600 : 3000;
        const Result r = RunOne(f, theirs[f], rounds);
        bad += r.bad;
        rounds_total += rounds;
        calls += r.calls;
        bof3::Log("shadow      battle_sprites %-26s %5u rounds, %6u stand-in calls, %3u paths, %u MISMATCHES", kClones[f].name,
                  rounds, r.calls, r.paths, r.bad);
        if (used < 800) used += std::snprintf(line + used, sizeof line - used, "%s%u", f ? "," : "", r.bad);
    }
    g = kOriginals;

    std::memcpy(At(at::kBossHandlers), saved_bosses, sizeof saved_bosses);
    SetLong(At(at::kEnemyOffsets), saved_offsets);
    Field_State = saved_field;
    Apply(saved, kAll);
    Sprite_Current = saved_current;

    bof3::Log("shadow      battle_sprites self-test: %u functions, %u rounds, %u calls to the stand-ins, %u MISMATCHES "
              "(per function: %s)", (unsigned)kFunctions, rounds_total, calls, bad, line);
    if (bad) bof3::Fatal("the battle sprite functions differ from the originals in %u self-test rounds", bad);
}

}  // namespace battle_sprites
