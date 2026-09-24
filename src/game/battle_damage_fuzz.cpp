// BOF3X_SHADOW=battle_damage: a differential fuzz of the damage chain, the
// effect result and its amounts, the affinity, the turn order and the AI
// conditions, once at start-up. docs/battle_damage.md section 5.
//
// Eighteen byte-copies, every call out re-aimed at a recording stand-in, the
// AI condition table relocated in its copy, the effect handler table 0x64E73C
// (130 entries) swapped for 130 recording stand-ins and the item category
// table 0x64E72C for byte lists of our own. One round: one function, random
// bytes in every region any of them touches, then that function's branch
// boundaries seeded; theirs, then from the same state ours; the regions, the
// answer (at the width the original defines) and the stand-ins' log compared.
//
// The stand-ins are as loud as the real callees where the caller reads after
// the call: each scribbles on one of the cells its callers read again (the
// round's two actors' HP, maxima, flags, status, weapon and family bytes, the
// damage scratch, the battle flags, the turn work, the AI's script byte and
// counters), and the effect handlers write the result record's deltas, may
// move its pointer and may change the target. Rand answers only what the
// CRT's rand can (0..0x7FFF): Battle_CalcDamage indexes its stack by it.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/battle_damage_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_damage {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x6C8E9CF5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }
std::uint32_t Byte(std::uint32_t low) { return Half() ? low : Garbage(0xFF, low & 0xFF); }

constexpr unsigned kLog = 512;
struct Entry { std::uint32_t what, a, b, c, d, e; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;   // the stand-ins' own stream: the same on both passes

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}

unsigned char* PartyAt(unsigned a) { return At(at::kParty + (a & 0xFF) * at::kPartyStride); }
unsigned char* EnemyAt(unsigned a) { return At(at::kEnemy + ((a & 0xFF) - 3) * at::kEnemyStride); }

// The round's two actors: the stand-ins scribble on their fields.
unsigned g_actor[2];

// A cell some caller reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const auto v = static_cast<unsigned char>(h >> 12);
    const unsigned bit = 1u << ((h >> 20) % 8);
    const unsigned who = g_actor[(h >> 23) & 1] & 0xFF;
    const bool party = who <= 2;
    unsigned char* const r = party ? PartyAt(who) : EnemyAt(who);
    switch ((h >> 4) % 24) {
    case 0: r[party ? at::kPHp : at::kEHp] = v; break;
    case 1: r[(party ? at::kPHp : at::kEHp) + 1] ^= static_cast<unsigned char>(bit); break;
    case 2: r[party ? at::kPMaxHp : at::kEMaxHp] = v; break;
    case 3: r[party ? at::kPAp : at::kEAp] = v; break;
    case 4: r[(party ? at::kPFlags : at::kEFlags) + ((h >> 26) % 3)] ^= static_cast<unsigned char>(bit); break;
    case 5: r[party ? at::kPFlags2 : at::kEFlags2] ^= static_cast<unsigned char>(bit); break;
    case 6: r[party ? at::kPStatus : at::kEStatus] ^= static_cast<unsigned char>(bit); break;
    case 7: if (party) r[at::kPWeapon] = static_cast<unsigned char>("JLO\x13\x16"[(h >> 26) % 5]); break;
    case 8: if (party) r[at::kPWeaponKind] = static_cast<unsigned char>(v & 1 ? 5 : v); else r[at::kEFamily] = v & 1 ? 4 : 1; break;
    case 9: r[party ? at::kPSurvive : at::kEHoly] = v; break;
    case 10: At(bof3::addr::DamageScratch)[0] ^= static_cast<unsigned char>(bit); break;
    case 11: At(at::kBattleFlags)[0] ^= 0x80; break;
    case 12: At(at::kPartyCut)[0] = v & 1 ? 2 : v; break;
    case 13: At(at::kCuredBits + (v & 1))[0] = v; break;
    case 14: At(at::kAttackerAtk + (v & 1))[0] = static_cast<unsigned char>(h >> 24); break;
    case 15: At(at::kAttackerInt + (v & 1))[0] = static_cast<unsigned char>(h >> 24); break;
    case 16: At(at::kTurnWork + (h >> 24) % 44)[0] = v; break;
    case 17: At(at::kEntryCount)[0] = static_cast<unsigned char>(v % 3); break;
    case 18: r[party ? at::kPAgi : at::kEAgi] = v; break;
    case 19: if (!party) r[at::kEObject + 0xF0] = static_cast<unsigned char>(v % 16); break;
    case 20: At(at::kTurnCounter)[0] = static_cast<unsigned char>(v % 12); break;
    case 21: At(at::kAiByteB1 + v % 3)[0] = static_cast<unsigned char>(h >> 26); break;
    case 22: At(at::kAiScripts + (h >> 16) % 0x900)[0] = static_cast<unsigned char>(0xB + v % 0x1D); break;
    default: r[party ? at::kPCommand : at::kECommand] = v; break;
    }
}

// --- the stand-ins ---------------------------------------------------------

unsigned char Flag(std::uint32_t h) { return static_cast<unsigned char>(h % 2); }
std::uint32_t Stale(std::uint32_t h, std::uint32_t low, std::uint32_t mask) { return (h & ~mask) | (low & mask); }

unsigned char __cdecl StubCanAct(unsigned actor) {
    Record(1, actor & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(Stale(h, Flag(h >> 3), 0xFF));
}
unsigned char __cdecl StubCanCommand(unsigned actor) {
    Record(2, actor & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(Stale(h, Flag(h >> 3), 0xFF));
}
unsigned char __cdecl StubLevelClass(unsigned level, unsigned row) {
    Record(3, level & 0xFF, row & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    // the turn work's value words, which Battle_BuildTurnOrder reads after this call
    if (h % 3 == 0)
        for (unsigned i = 0; i < 11; ++i) SetWord(At(at::kTurnWork + 4 * i), (h >> (i % 16)) * 0x9E37u);
    return static_cast<unsigned char>(h % 8 == 0 ? 7 + (h >> 4) % 3 : (h >> 4) % 7);
}
int TargetHp() {
    const unsigned t = g_actor[1] & 0xFF;
    return t <= 2 ? Word(PartyAt(t) + at::kPHp) : Word(EnemyAt(t) + at::kEHp);
}
short __cdecl StubCalcDamage(unsigned attacker, unsigned target, unsigned element) {
    Record(4, attacker, target, element);
    const int hp = TargetHp();
    Disturb();
    const std::uint32_t h = Hash();
    static const short kValues[] = {0, 1, -1, 9999, -9999, 0x7FFF, -0x8000, 2, -2};
    switch (h % 4) {
    case 0: return kValues[(h >> 4) % 9];
    case 1: return static_cast<short>(hp + static_cast<int>((h >> 4) % 3) - 1);
    case 2: return static_cast<short>(-static_cast<int>((h >> 4) % 200));
    default: return static_cast<short>(h >> 8);
    }
}
int WideValue(std::uint32_t h) {
    static const int kValues[] = {0, 1, -1, 2, 0xFFFF, 0x10000, -0x10000, 0x7FFF, 0x8000, -0x8000, 9999, 10000, -10000, 0x7FFFFFFF};
    switch (h % 4) {
    case 0: return kValues[(h >> 4) % 14];
    case 1: return static_cast<int>((h >> 4) % 1200);
    case 2: return static_cast<int>(h) >> static_cast<int>((h >> 3) % 24);
    default: return static_cast<int>(h >> 12) - 0x40000;
    }
}
int __cdecl StubBaseDamage(unsigned attacker, unsigned target, unsigned mode) {
    Record(5, attacker, target, mode);
    Disturb();
    return WideValue(Hash());
}
int __cdecl StubScaleDamage(unsigned attacker, unsigned target, int base) {
    Record(6, attacker, target, static_cast<std::uint32_t>(base));
    Disturb();
    return WideValue(Hash());
}
int Affinity(std::uint32_t h) {
    static const short kValues[] = {0, 100, -100, 200, 50, -1, 0x7FFF, -0x8000, 300, 1};
    return static_cast<int>((h & 0xFFFF0000u) | static_cast<unsigned short>(h % 3 ? kValues[(h >> 4) % 10] : static_cast<short>(h >> 3)));
}
int __cdecl StubElementAffinity(unsigned target, unsigned mask) {
    Record(7, target, mask);
    Disturb();
    if (Hash() % 3 == 0) At(bof3::addr::DamageScratch)[0] ^= 0x20;   // read again after this call
    return Affinity(Hash());
}
unsigned char __cdecl StubRowDone(const unsigned char* enemy, unsigned row) {
    Record(8, Address(enemy), row & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(Stale(h, Flag(h >> 5), 0xFF));
}
unsigned char __cdecl StubIsOut(unsigned actor) {
    Record(9, actor & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(Stale(h, h % 3 == 0, 0xFF));
}
void __cdecl StubShowMessage(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e) {
    Record(10, a, b, c, d, e);
    Disturb();
}
unsigned char __cdecl StubResisted(unsigned attacker, unsigned target) {
    Record(11, attacker, target);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(Stale(h, Flag(h >> 3), 0xFF));
}
void __cdecl StubInflict(unsigned target, unsigned status) {
    Record(12, target, status);
    Disturb();
}
void __cdecl StubCure(unsigned id, unsigned status) {
    Record(13, id & 0xFF, status);
    Disturb();
}
int __cdecl StubHitParty(int amount, unsigned attacker, unsigned target) {
    Record(14, static_cast<std::uint32_t>(amount), attacker, target);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 3 == 0 ? amount : WideValue(h);
}
int __cdecl StubHitEnemy(int amount, unsigned attacker, unsigned target) {
    Record(15, static_cast<std::uint32_t>(amount), attacker, target);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 3 == 0 ? amount : WideValue(h);
}
int __cdecl StubPartyDef() {
    Record(16);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<int>(h % 2 ? (h & 0xFFFF0000u) | Word(At(at::kAttackerAtk)) : h >> 4);
}
int __cdecl StubPsiAffinity(unsigned target, unsigned mask) {
    Record(17, target, mask);
    Disturb();
    return Affinity(Hash() ^ 0x5A5A0000u);
}
int __cdecl StubRand() {
    Record(18);
    Disturb();
    const std::uint32_t h = Hash();
    static const int kValues[] = {0, 1, 2, 3, 6, 7, 8, 15, 16, 0x7FFF, 0x7FF8};
    return h % 3 == 0 ? kValues[(h >> 4) % 11] : static_cast<int>((h >> 7) & 0x7FFF);
}
void __cdecl StubAiApply(unsigned char* enemy, const unsigned char* row) {
    Record(19, Address(enemy), Address(row));
    Disturb();
}
void __cdecl StubAiSetDone(unsigned char* enemy, unsigned row, unsigned on) {
    Record(20, Address(enemy), row & 0xFF, on & 0xFF);
    Disturb();
}
unsigned char __cdecl StubAiRowsLeft(unsigned row, unsigned enemy) {
    Record(21, row & 0xFF, enemy & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(Stale(h, Flag(h >> 3), 0xFF));
}
void __cdecl StubAiFinish() {
    Record(22);
    Disturb();
}

// The effect handlers: each logs its index and leaves deltas in the result
// record - around the clamps, around the target's HP and AP, zero - and may
// move the record's pointer or the target.
unsigned char g_bufs[0x40];   // +0 the item command, +0x10 and +0x20 two result records

template <unsigned N> void __cdecl StubHandler() {
    Record(100 + N);
    std::uint32_t h = Hash();
    const unsigned t = g_actor[1] & 0xFF;
    const int hp = t <= 2 ? Word(PartyAt(t) + at::kPHp) : Word(EnemyAt(t) + at::kEHp);
    const int ap = t <= 2 ? Word(PartyAt(t) + at::kPAp) : Word(EnemyAt(t) + at::kEAp);
    static const short kValues[] = {0, 1, -1, 9999, 10000, -9999, -10000, 0x7FFF, -0x8000, 9998, -9998};
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned base = k ? static_cast<unsigned>(ap) : static_cast<unsigned>(hp);
        short v;
        switch ((h >> (4 * k)) % 6) {
        case 0: v = kValues[(h >> (8 + 4 * k)) % 11]; break;
        case 5:
        case 1: v = static_cast<short>(base + (h >> (12 + k)) % 3 - 1); break;
        case 2: v = static_cast<short>(-static_cast<int>((h >> (10 + k)) % 300)); break;
        case 3: v = 0; break;
        default: v = static_cast<short>(h >> (14 + k)); break;
        }
        SetWord(At(static_cast<std::uint32_t>(Long(At(at::kResult)))) + 4 + 2 * k, static_cast<unsigned short>(v));
    }
    h = Hash() ^ 0x77777777u;
    if (h % 5 == 0) SetLong(At(at::kResult), static_cast<std::int32_t>(Address(g_bufs + (h & 0x100 ? 0x10 : 0x20))));
    if (h % 7 == 0) {
        g_actor[1] = (h >> 8) % 12;
        At(at::kTarget)[0] = static_cast<unsigned char>(g_actor[1]);
    }
    Disturb();
}
using Handler = void (__cdecl*)();
template <std::size_t... I> constexpr std::array<Handler, sizeof...(I)> MakeHandlers(std::index_sequence<I...>) {
    return {&StubHandler<static_cast<unsigned>(I)>...};
}
constexpr std::array<Handler, at::kHandlerCount> kHandlerStubs = MakeHandlers(std::make_index_sequence<at::kHandlerCount>{});

const Callees kStubs = {
    StubCanAct, StubCanCommand, StubLevelClass, StubCalcDamage, StubBaseDamage, StubScaleDamage, StubElementAffinity,
    StubRowDone, StubIsOut, StubShowMessage, StubResisted, StubInflict, StubCure, StubHitParty, StubHitEnemy, StubPartyDef,
    StubPsiAffinity, StubRand, StubAiApply, StubAiSetDone, StubAiRowsLeft, StubAiFinish,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x445980: return f(&StubCanAct);
    case 0x4458B0: return f(&StubCanCommand);
    case 0x445640: return f(&StubLevelClass);
    case 0x445CF0: return f(&StubCalcDamage);
    case 0x4462B0: return f(&StubBaseDamage);
    case 0x446430: return f(&StubScaleDamage);
    case 0x44EE80: return f(&StubElementAffinity);
    case 0x44B2C0: return f(&StubRowDone);
    case 0x4456C0: return f(&StubIsOut);
    case 0x44A650: return f(&StubShowMessage);
    case 0x44FA70: return f(&StubResisted);
    case 0x44F1D0: return f(&StubInflict);
    case 0x44F4B0: return f(&StubCure);
    case 0x446110: return f(&StubHitParty);
    case 0x4461B0: return f(&StubHitEnemy);
    case 0x4463E0: return f(&StubPartyDef);
    case 0x44F030: return f(&StubPsiAffinity);
    case 0x5B93D2: return f(&StubRand);
    case 0x44B3A0: return f(&StubAiApply);
    case 0x44B2E0: return f(&StubAiSetDone);
    case 0x44B240: return f(&StubAiRowsLeft);
    case 0x44B920: return f(&StubAiFinish);
    default: bof3::Fatal("battle_damage: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the eighteen copies (capstone recursive descent, 2026-09-23: every jump
// internal; the calls below are every call that leaves, the one table the
// AI's) ---

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    move_script::Table table;   // entries 0: none
    unsigned ret;               // the answer's width the original defines: 0 none, 1 al, 2 ax, 4 eax
    const void* ours;
};

constexpr Call kEntryOrder[] = {{0x2C, 0x445980}, {0x7E, 0x4458B0}, {0x164, 0x4458B0}};
constexpr Call kTurnOrder[] = {{0x2F, 0x445980}, {0x82, 0x445980}, {0x107, 0x445640},
                               {0x183, 0x445980}, {0x1A1, 0x445640}, {0x1B4, 0x5B93D2}};
constexpr Call kDefault[] = {{0x18, 0x4456C0}, {0x44, 0x4456C0}, {0x6C, 0x4456C0}, {0x95, 0x4456C0}};
constexpr Call kApply[] = {{0x49, 0x445CF0}, {0x8C, 0x44A650}, {0x188, 0x44A650}, {0x247, 0x44FA70}, {0x256, 0x44F1D0},
                           {0x26D, 0x44FA70}, {0x27C, 0x44F1D0}, {0x293, 0x44FA70}, {0x2A2, 0x44F1D0}};
constexpr Call kCalc[] = {{0x12B, 0x4462B0}, {0x22F, 0x44F4B0}, {0x254, 0x44F4B0}, {0x29A, 0x44F4B0}, {0x2BE, 0x44F4B0},
                          {0x2F4, 0x446110}, {0x2FB, 0x4461B0}, {0x30C, 0x5B93D2}, {0x34D, 0x4462B0}};
constexpr Call kBase[] = {{0x50, 0x4463E0}, {0x95, 0x5B93D2}, {0x10E, 0x5B93D2}, {0x11F, 0x446430}};
constexpr Call kScale[] = {{0x35, 0x5B93D2}, {0x60, 0x44EE80}};
constexpr Call kAi[] = {{0x12, 0x4456C0}, {0x10D, 0x4456C0}, {0x29B, 0x44B240}, {0x2A9, 0x44B2C0}, {0x2B7, 0x44B3A0},
                        {0x2C0, 0x44B2E0}, {0x2CA, 0x44B2C0}, {0x2E3, 0x44B2C0}, {0x2F3, 0x44B2E0}, {0x320, 0x44B920}};
constexpr Call kSkill[] = {{0x8B, 0x44F030}, {0x92, 0x44EE80}, {0xB5, 0x5B93D2}};

#define BD_C(name, base, size, calls, table, ret) \
    {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), table, ret, reinterpret_cast<const void*>(&::name)}
#define BD_P(name, base, size, table, ret) {#name, base, size, nullptr, 0, table, ret, reinterpret_cast<const void*>(&::name)}
constexpr move_script::Table kNoTable = {0, 0, 0};
const Clone kClones[] = {
    BD_C(Battle_BuildEntryOrder, 0x444F40, 0x197, kEntryOrder, kNoTable, 0),
    BD_C(Battle_BuildTurnOrder, 0x4450E0, 0x2D7, kTurnOrder, kNoTable, 0),
    BD_P(Battle_LevelClass, 0x445640, 0x3D, kNoTable, 1),
    BD_P(Battle_ClearCommands, 0x445680, 0x31, kNoTable, 0),
    BD_C(Battle_DefaultTarget, 0x445730, 0xB3, kDefault, kNoTable, 1),
    BD_P(Battle_ActorCanCommand, 0x4458B0, 0xCE, kNoTable, 1),
    BD_P(Battle_ActorCanAct, 0x445980, 0xB0, kNoTable, 1),
    BD_C(Battle_ApplyDamage, 0x445A30, 0x2B1, kApply, kNoTable, 2),
    BD_C(Battle_CalcDamage, 0x445CF0, 0x41B, kCalc, kNoTable, 2),
    BD_C(Battle_BaseDamage, 0x4462B0, 0x12A, kBase, kNoTable, 4),
    BD_C(Battle_ScaleDamage, 0x446430, 0x101, kScale, kNoTable, 4),
    BD_P(Battle_RemoveFromTurnOrder, 0x446650, 0xA9, kNoTable, 0),
    BD_C(EnemyAI_ChooseActions, 0x44AE90, 0x3A4, kAi, (move_script::Table{0x86, 0x330, 29}), 0),
    BD_P(EnemyAI_RowDone, 0x44B2C0, 0x13, kNoTable, 1),
    BD_P(Effect_ApplyResult, 0x44B9F0, 0x4D6, kNoTable, 0),
    BD_C(Effect_SkillDamage, 0x44ED10, 0x166, kSkill, kNoTable, 4),
    BD_P(Battle_ElementAffinity, 0x44EE80, 0x1AF, kNoTable, 4),
    BD_P(Effect_HealAmount, 0x44F130, 0x9A, kNoTable, 4),
};
#undef BD_C
#undef BD_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kEntryK, kTurnK, kLevelK, kClearK, kDefaultK, kCommandK, kActK, kApplyK, kCalcK, kBaseK, kScaleK, kRemoveK, kAiK,
    kRowDoneK, kResultK, kSkillK, kElementK, kHealK,
};
static_assert(kHealK + 1 == kCount, "the index enum follows kClones");

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
constexpr unsigned kRegionCount = 7;
Region g_regions[kRegionCount];
constexpr unsigned kRegionBytes = 0x4C0 + 4 + 0xC00 + 0x580 + 0xBA0 + 0x900 + sizeof g_bufs;

void MakeRegions() {
    g_regions[0] = {at::kParty - 0x10, 0x4C0};         // ObjTrio: the three party objects, to 0x8031FF (0x8031F3)
    g_regions[1] = {bof3::addr::DamageScratch, 4};
    g_regions[2] = {0x904000, 0xC00};                  // 0x904060, the battle state 0x904AA8..0x904B9B
    g_regions[3] = {at::kTurnWork, 0x580};             // the turn work, the damage scratch stats 0x939F86..0x939FEB
    g_regions[4] = {at::kEnemy - 0x80, 0xBA0};         // the enemy objects and records, room for actor 11
    g_regions[5] = {at::kAiScripts, 0x900};            // AI scripts 0..15
    g_regions[6] = {Address(g_bufs), sizeof g_bufs};
}

struct State {
    unsigned char memory[kRegionBytes];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned off = 0;
    for (const Region& r : g_regions) { std::memcpy(s.memory + off, At(r.at), r.size); off += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned off = 0;
    for (const Region& r : g_regions) { std::memcpy(At(r.at), s.memory + off, r.size); off += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

// The item categories: our own byte lists, every entry a real handler index.
unsigned char g_categories[4][256];
std::uint32_t g_saved_categories[4];
std::uint32_t g_saved_handlers[at::kHandlerCount];

struct Args { std::uint32_t a[6]; unsigned actor[2]; };

unsigned Actor() { return Next() % 12; }
std::uint32_t ActorArg(unsigned actor) { return Byte(actor); }

// The fields the round's actors are tested on, near their boundaries.
void SeedActor(unsigned a) {
    const bool party = a <= 2;
    unsigned char* const r = party ? PartyAt(a) : EnemyAt(a);
    static const std::uint16_t kHp[] = {0, 1, 2, 9, 10, 100, 999, 9999, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF};
    if (Often()) SetWord(r + (party ? at::kPHp : at::kEHp), kHp[Next() % 12]);
    if (Half()) SetWord(r + (party ? at::kPAp : at::kEAp), kHp[Next() % 12]);
    if (Half()) SetWord(r + (party ? at::kPMaxHp : at::kEMaxHp), Word(r + (party ? at::kPHp : at::kEHp)) + Next() % 3 - 1);
    if (Half()) SetWord(r + (party ? at::kPMaxAp : at::kEMaxAp), Word(r + (party ? at::kPAp : at::kEAp)) + Next() % 3 - 1);
    const unsigned mask = static_cast<unsigned>(Next() & Next());   // sparse flag bits
    if (Half()) SetLong(r + (party ? at::kPFlags : at::kEFlags), static_cast<std::int32_t>(Half() ? mask : 0));
    if (Half()) SetLong(r + (party ? at::kPFlags2 : at::kEFlags2), static_cast<std::int32_t>(Half() ? Next() & Next() : 0));
    if (Half()) SetWord(r + (party ? at::kPStatus : at::kEStatus), Half() ? 0 : Next() & Next());
    if (Often()) (party ? r[at::kPPresent] : r[at::kEPresent]) |= 1;
    if (party) {
        if (Half()) r[at::kPWeaponKind] = 5;
        if (Half()) r[at::kPWeapon] = static_cast<unsigned char>("JLO\x13\x17\x33\x16\x35\x40"[Next() % 9]);
        if (Half()) r[at::kPSurvive] = static_cast<unsigned char>(Next() % 8);
        if (Half()) r[at::kPHoly] = static_cast<unsigned char>(Next() % 8);
    } else {
        if (Half()) r[at::kEFamily] = Half() ? 4 : 1;
        if (Half()) r[at::kEHoly] = static_cast<unsigned char>(Next() % 8);
        if (Half()) r[at::kEObject + 0xF0] = static_cast<unsigned char>(Next() % 16);
    }
}

// Every round: the pointers valid, the target and the actor in range.
void SeedCommon() {
    SetLong(At(at::kItemCommand), static_cast<std::int32_t>(Address(g_bufs)));
    SetLong(At(at::kResult), static_cast<std::int32_t>(Address(g_bufs + (Half() ? 0x10 : 0x20))));
    At(at::kTarget)[0] = static_cast<unsigned char>(Actor());
    At(at::kActing)[0] = static_cast<unsigned char>(Actor());
    g_bufs[3] = static_cast<unsigned char>(Next() % 4);   // the item command's category
    if (Half()) SetWord(At(at::kAbility), Half() ? Next() % 227 : Next() % 0x1EC);
    else SetWord(At(at::kAbility), 0xA3);
}

Args Seed(unsigned k) {
    Args x;
    for (auto& v : x.a) v = Next();
    SeedCommon();
    x.actor[0] = Actor();
    x.actor[1] = Actor();
    if (Half()) x.actor[1] = x.actor[0] <= 2 ? 3 + Next() % 8 : Next() % 3;
    if (Half()) At(at::kBattleFlags)[0] = static_cast<unsigned char>(Half() ? 0x80 : 0);
    if (Half()) At(at::kPartyCut)[0] = static_cast<unsigned char>(Half() ? 2 : Next() % 4);
    if (Half()) At(at::kGate)[0] = static_cast<unsigned char>(Half() ? 0 : 1);
    if (Half()) At(at::kGate2)[0] = static_cast<unsigned char>(Half() ? 0 : 1);
    if (Half()) At(at::kSideOut)[0] = static_cast<unsigned char>(Next() % 5);
    switch (k) {
    case kEntryK:
    case kTurnK:
        for (unsigned a = 0; a < 11; ++a) {
            unsigned char* const r = a <= 2 ? PartyAt(a) : EnemyAt(a);
            if (Half()) SetLong(r + (a <= 2 ? at::kPFlags2 : at::kEFlags2), Half() ? 0x8000 : 0);
            static const std::uint16_t kAgi[] = {0, 1, 50, 50, 0x7FFF, 0x8000, 0xFFFF, 100};
            if (Often()) SetWord(r + (a <= 2 ? at::kPAgi : at::kEAgi), kAgi[Next() % 8]);
            if (a <= 2) {
                static const unsigned char kCommands[] = {0, 4, 4, 5, 1, 3};
                r[at::kPCommand] = kCommands[Next() % 6];
                SetWord(r + at::kPCommandId, Next() % 300);
            }
        }
        break;
    case kLevelK: {
        static const unsigned char kLevels[] = {0, 7, 8, 15, 16, 31, 32, 35, 36, 47, 48, 63, 64, 98, 99, 100, 255};
        x.a[0] = Byte(kLevels[Next() % 17]);
        x.a[1] = Byte(Often() ? Next() % 2 : Next());
        break;
    }
    case kDefaultK:
        x.a[0] = Byte(Often() ? Next() % 12 : Next());
        break;
    case kCommandK:
    case kActK: {
        if (Half()) x.actor[0] = Next() % 3;
        SeedActor(x.actor[0]);
        x.a[0] = ActorArg(x.actor[0]);
        if (Half()) {
            // one who can act, then one refusal at a time
            unsigned char* const r = x.actor[0] <= 2 ? PartyAt(x.actor[0]) : EnemyAt(x.actor[0]);
            const bool party = x.actor[0] <= 2;
            (party ? r[at::kPPresent] : r[at::kEPresent]) |= 1;
            static const unsigned char kStatusBits[] = {2, 5, 6, 8, 11, 14, 3, 12};
            static const unsigned char kFlagBits[] = {0, 14, 16, 4, 1, 15};
            SetWord(r + (party ? at::kPStatus : at::kEStatus), Half() ? 0 : 1u << kStatusBits[Next() % 8]);
            SetLong(r + (party ? at::kPFlags2 : at::kEFlags2),
                    static_cast<std::int32_t>(0x10 | (Half() ? 0 : 1u << kFlagBits[Next() % 6])));
            At(at::kSideOut)[0] = static_cast<unsigned char>(Half() ? 0 : Next() % 5);
        }
        break;
    }
    case kRemoveK: {
        x.a[0] = ActorArg(x.actor[0]);
        At(at::kTurnCount)[0] = static_cast<unsigned char>(Often() ? Next() % 12 : Next());
        for (unsigned i = 0; i < 11; ++i)
            if (Half()) At(at::kTurnOrder + i)[0] = static_cast<unsigned char>(x.actor[0]);
        break;
    }
    case kApplyK:
    case kCalcK:
    case kBaseK: {
        if (k == kApplyK && Half()) {
            // a party attacker whose weapon's element rolls apply
            x.actor[0] = Next() % 3;
            PartyAt(x.actor[0])[at::kPWeaponKind] = 5;
            PartyAt(x.actor[0])[at::kPWeapon] = static_cast<unsigned char>("JLO"[Next() % 3]);
        }
        if (k == kCalcK && Half()) {
            // a party weapon on an enemy family, one status bit
            x.actor[0] = Next() % 3;
            x.actor[1] = 3 + Next() % 8;
            PartyAt(x.actor[0])[at::kPWeapon] = static_cast<unsigned char>("\x13\x17\x33\x16\x35\x40\x41"[Next() % 7]);
            EnemyAt(x.actor[1])[at::kEFamily] = static_cast<unsigned char>(Next() % 6);
        }
        SeedActor(x.actor[0]);
        SeedActor(x.actor[1]);
        if (k == kCalcK && Half()) {
            static const unsigned char kBits[] = {0x20, 0x40, 4, 0x80, 0x60, 0};
            unsigned char* const t = x.actor[1] <= 2 ? PartyAt(x.actor[1]) + at::kPStatus : EnemyAt(x.actor[1]) + at::kEStatus;
            t[0] = kBits[Next() % 6];
            // flag bit 1's roll, which can take a 1 back to 0 for the minimum-1 tests
            unsigned char* const f = x.actor[1] <= 2 ? PartyAt(x.actor[1]) + at::kPFlags : EnemyAt(x.actor[1]) + at::kEFlags;
            if (Half()) f[0] = static_cast<unsigned char>((f[0] | 2) & ~1u);
            if (Half()) At(at::kBattleFlags)[0] = 0;
        }
        if (k == kApplyK && Half() && x.actor[1] <= 2) PartyAt(x.actor[1])[at::kPSurvive] = static_cast<unsigned char>(Next() % 3);
        x.a[0] = ActorArg(x.actor[0]);
        x.a[1] = ActorArg(x.actor[1]);
        if (k == kCalcK) x.a[2] = Often() ? Garbage(0xFFFF, 0xFFFF) : Next() & (Half() ? 0x3F : 0xFFFF);
        if (k == kBaseK) x.a[2] = Byte(Next() % 2);
        if (Half()) {
            // the attack against the defence: equal, one either side
            const unsigned atk = Next() % 400;
            SetWord(At(at::kAttackerAtk), atk);
            SetWord(At(at::kTargetDef), atk + Next() % 3 - 1);
        }
        break;
    }
    case kScaleK: {
        static const std::int32_t kBases[] = {0, 1, 2, 100, 999, 1000, 1001, 1999, 2000, 255999, 256000, -1, -1000, 0x7FFFFF, 0x800000};
        x.a[1] = ActorArg(x.actor[1]);
        x.a[2] = static_cast<std::uint32_t>(Often() ? kBases[Next() % 15] : static_cast<std::int32_t>(Next()) >> (Next() % 24));
        SeedActor(x.actor[1]);
        At(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(Half() ? (Half() ? 0x20 : 0x01) | (Half() ? 0x20 : 0) : Next());
        break;
    }
    case kAiK:
        for (unsigned e = 0; e < 8; ++e) {
            unsigned char* const obj = EnemyAt(e + 3) + at::kEObject;
            if (Often()) obj[0xF0] = static_cast<unsigned char>(Next() % 16);
            if (Half()) SetWord(obj + 0xA4, Word(obj + 0xB0) / (Half() ? 4 : 2) + Next() % 3 - 1);
            if (Half()) SetWord(obj + 0xA6, Word(obj + 0xB2) / (Half() ? 5 : 2) + Next() % 3 - 1);
            if (Half()) SetWord(obj + 0x98, PartyAt(0)[at::kPLevel] - 5 + Next() % 3 - 1);
        }
        for (unsigned i = 0; i < 0x900; i += 16)
            if (Often()) At(at::kAiScripts + i)[0] = static_cast<unsigned char>(0xB + Next() % 0x1D);
        if (Half()) SetLong(At(at::kTurnCounter), static_cast<std::int32_t>(Next() % 13 + (Half() ? 0 : (Next() % 4) << 8)));
        for (unsigned i = 0; i < 3; ++i)
            if (Half()) At(at::kAiByteB1 + i)[0] = static_cast<unsigned char>(Next() % 3);
        if (Half()) At(at::kAiByteB97)[0] = static_cast<unsigned char>(Next() % 2);
        break;
    case kRowDoneK: {
        x.a[0] = Often() ? Address(EnemyAt(3 + Next() % 8) + at::kEObject) : at::kEnemy - 0x80 + Next() % 0xA00;
        x.a[1] = Byte(Often() ? Next() % 10 : Next() % 40);
        break;
    }
    case kResultK:
        SeedActor(x.actor[1]);
        SeedActor(At(at::kActing)[0]);
        At(at::kTarget)[0] = static_cast<unsigned char>(x.actor[1]);
        if (Half()) At(at::kActing + 1)[0] = 4;
        break;
    case kSkillK:
    case kHealK:
    case kElementK:
        SeedActor(x.actor[1]);
        x.a[0] = ActorArg(x.actor[0]);
        x.a[1] = ActorArg(x.actor[1]);
        if (k == kElementK) {
            x.a[0] = x.a[1];
            x.a[1] = Byte(Next() & 0x3F);
            unsigned char* const c = x.actor[1] <= 2 ? PartyAt(x.actor[1]) + at::kPElements : EnemyAt(x.actor[1]) + at::kEElements;
            for (unsigned i = 0; i < 5; ++i)
                if (Often()) c[i] = static_cast<unsigned char>(Next() % 16);
        }
        if (k == kSkillK) {
            x.a[2] = Often() ? Next() % 300 : Next();
            x.a[3] = Byte(Next() % 2);
            if (Half()) SetWord(At(at::kTargetInt), 245 + Next() % 12);   // 100 - INT / 5 around 50
            if (Half()) SetWord(At(at::kAttackerInt), Next() % 300);
            if (Half()) At(at::kTarget)[0] = static_cast<unsigned char>(Next() % 4);
        }
        break;
    default: break;
    }
    return x;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[23], handlers;
    unsigned answers[kCount][3];   // answer 0, 1, other
    unsigned kills, heals, clamps, immune;
} g_cover;

void Cover(unsigned k, const State& in, const State& out, std::uint32_t result) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        if (out.log[i].what < 23) ++g_cover.logged[out.log[i].what];
        else ++g_cover.handlers;
    }
    ++g_cover.answers[k][result == 0 ? 0 : result == 1 ? 1 : 2];
    if (k == kCalcK && (result == 9999 || result == 0xD8F1)) ++g_cover.clamps;
    (void)in;
}

using Fn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                    std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                    std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    MakeRegions();
    unsigned region_bytes = 0;
    for (const Region& r : g_regions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_damage: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("battle_damage: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, c.table);
    }

    // Save what the fuzz overwrites outside the regions, and swap the tables.
    static State saved, input, their_out, our_out;
    Capture(saved);
    for (unsigned i = 0; i < 4; ++i) {
        g_saved_categories[i] = static_cast<std::uint32_t>(Long(At(at::kItemHandlers + 4 * i)));
        for (unsigned j = 0; j < 256; ++j) g_categories[i][j] = static_cast<unsigned char>(Next() % at::kHandlerCount);
        SetLong(At(at::kItemHandlers + 4 * i), static_cast<std::int32_t>(Address(g_categories[i])));
    }
    for (unsigned i = 0; i < at::kHandlerCount; ++i) {
        g_saved_handlers[i] = static_cast<std::uint32_t>(Long(At(at::kHandlers + 4 * i)));
        SetLong(At(at::kHandlers + 4 * i), static_cast<std::int32_t>(Address(reinterpret_cast<const void*>(kHandlerStubs[i]))));
    }
    g = kStubs;

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        g_seed = Next();
        const Args x = Seed(k);
        Capture(input);

        std::uint32_t result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g_actor[0] = x.actor[0];
            g_actor[1] = x.actor[1];
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            const std::uint32_t r = reinterpret_cast<Fn>(const_cast<void*>(fn))(
                x.a[0], x.a[1], x.a[2], x.a[3], x.a[4], x.a[5], 0x13579BDFu, 0x2468ACE0u, 0x0F1E2D3Cu, 0x4B5A6978u,
                0x8796A5B4u, 0xC3D2E1F0u, 0x01234567u, 0x89ABCDEFu, 0xFEDCBA98u, 0x76543210u);
            Capture(out);
            const unsigned w = kClones[k].ret;
            result[pass] = w == 0 ? 0u : w == 1 ? (r & 0xFFu) : w == 2 ? (r & 0xFFFFu) : r;
            out.result = result[pass];
        }
        calls += their_out.log_n;
        Cover(k, input, their_out, result[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_damage self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    for (unsigned i = 0; i < at::kHandlerCount; ++i) SetLong(At(at::kHandlers + 4 * i), static_cast<std::int32_t>(g_saved_handlers[i]));
    for (unsigned i = 0; i < 4; ++i) SetLong(At(at::kItemHandlers + 4 * i), static_cast<std::int32_t>(g_saved_categories[i]));
    Apply(saved);

    bof3::Log("shadow      battle_damage self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the party and enemy records, the battle state, the turn work, the AI scripts, the result "
              "records, the answer and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      battle_damage: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      battle_damage coverage: calls can-act %u, can-command %u, level class %u, calc %u, base %u, "
              "scale %u, element %u, row done %u, is-out %u, message %u, resisted %u, inflict %u, cure %u, hit %u / %u, "
              "party DEF %u, psi %u, rand %u, AI apply %u, set done %u, rows left %u, finish %u, handlers %u; "
              "CalcDamage clamped %u; answers 0/1/other: CanAct %u/%u, CanCommand %u/%u, DefaultTarget above 1 %u, "
              "ApplyDamage 0 %u, CalcDamage 0 %u, RowDone 1 %u",
              c.logged[1], c.logged[2], c.logged[3], c.logged[4], c.logged[5], c.logged[6], c.logged[7], c.logged[8],
              c.logged[9], c.logged[10], c.logged[11], c.logged[12], c.logged[13], c.logged[14], c.logged[15], c.logged[16],
              c.logged[17], c.logged[18], c.logged[19], c.logged[20], c.logged[21], c.logged[22], c.handlers, c.clamps,
              c.answers[kActK][0], c.answers[kActK][1], c.answers[kCommandK][0], c.answers[kCommandK][1],
              c.answers[kDefaultK][2], c.answers[kApplyK][0], c.answers[kCalcK][0], c.answers[kRowDoneK][1]);
    if (bad) bof3::Fatal("the damage and turn-order functions differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_damage
