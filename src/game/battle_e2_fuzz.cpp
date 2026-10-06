// BOF3X_SHADOW=battle_e2: group BE2's 48 battle-engine functions through the
// boss harness (boss_harness.h) as an engine group, once at start-up: three
// boss_harness::Runs - the effect tasks (kTask, the restore's and the
// markers' dispatchers with their state bytes drawn; Battle_BackupFlagged a
// kHelper), the action's begin (kHelper with the enemy index), the enemy ops
// (kState, the four dispatchers kDispatch over their .data tables).
// docs/battle_e2.md section 5.
//
// The clone rows are tools/band_rows.py's (--group BE2 --clones,
// 2026-09-29), each read against the disassembly.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_e2.h"
#include "game/battle_e2_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace battle_e2 {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using S = bh::Shape;
using A = bh::Answer;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BE2_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BE2_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BE2_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
            std::uint8_t state_at = 1, std::uint8_t states = 0, const bh::Imm* imms = nullptr, int n_imms = 0,
            const bh::JumpTable* tables = nullptr, int n_tables = 0) {
    bh::Clone c{name, base, size, calls, n, imms, n_imms, tables, n_tables, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group BE2 --clones)
// ===========================================================================

// --- the effect tasks
constexpr bh::CallSite kCalls433650[] = {{0x5B, 0x434870}, {0x6B, 0x4351F0}, {0xA2, 0x5891F0}, {0x103, 0x434730},
                                         {0x108, 0x5893A0}, {0x10D, 0x5890E0}, {0x11C, 0x4456C0}};
constexpr bh::CallSite kCalls433DA0[] = {{0x1, 0x454810},   {0x2D, 0x454DC0},  {0xBB, 0x435180},  {0x2F3, 0x5720C0},
                                         {0x3DE, 0x442310}, {0x3ED, 0x453300}, {0x4B2, 0x5891F0}, {0x4CD, 0x5366A0},
                                         {0x4EF, 0x446BB0}, {0x4F7, 0x4551A0}};
constexpr bh::Imm kImms434310[] = {{0xF, 0x434340}, {0x17, 0x4346C0}};
constexpr bh::CallSite kCalls434340[] = {{0x155, 0x5720C0}, {0x23E, 0x442310}, {0x24D, 0x453300}, {0x313, 0x5891F0},
                                         {0x32D, 0x5366A0}, {0x34F, 0x446BB0}, {0x357, 0x4551A0}};
constexpr bh::CallSite kCalls4346C0[] = {{0x68, 0x4351F0}};
constexpr bh::Imm kImms4348E0[] = {{0x19, 0x434930}, {0x24, 0x434A10}, {0x2C, 0x4AEE90}};
constexpr bh::CallSite kCalls434930[] = {{0xCA, 0x5891F0}, {0xD2, 0x5890E0}};
constexpr bh::CallSite kCalls434A10[] = {{0x42, 0x5893A0}, {0x47, 0x5890E0}};
constexpr bh::Imm kImms434A10[] = {{0xF, 0x434A60}, {0x17, 0x434B10}};
constexpr bh::Imm kImms434B90[] = {{0x19, 0x434BE0}, {0x24, 0x434C80}, {0x2C, 0x4AEE90}};
constexpr bh::CallSite kCalls434BE0[] = {{0x93, 0x5891F0}, {0x9B, 0x5890E0}};
constexpr bh::CallSite kCalls434C80[] = {{0x68, 0x5890E0}};
constexpr bh::Imm kImms434C80[] = {{0xF, 0x434D00}, {0x17, 0x434D30}};
constexpr bh::CallSite kCalls434D00[] = {{0x2, 0x5891F0}, {0x15, 0x5891F0}};
constexpr bh::Imm kImms434D70[] = {{0x19, 0x434DC0}, {0x24, 0x434E80}, {0x2C, 0x4AEE90}};
constexpr bh::CallSite kCalls434DC0[] = {{0x9A, 0x5891F0}};
constexpr bh::CallSite kCalls434E80[] = {{0xA0, 0x589330}, {0xA8, 0x5893A0}, {0xAD, 0x5890E0}};
constexpr bh::Imm kImms434F40[] = {{0x19, 0x434F90}, {0x24, 0x435050}, {0x2C, 0x4AEE90}};
constexpr bh::CallSite kCalls434F90[] = {{0x9A, 0x5891F0}};
constexpr bh::CallSite kCalls435050[] = {{0xA0, 0x589330}, {0xA8, 0x5893A0}, {0xAD, 0x5890E0}};

const bh::Clone kTasks[] = {
    C("BattleFx_WatchIcon", 0x433650, 0x140, kCalls433650, BE2_N(kCalls433650), BE2_FN(BattleFx_WatchIcon), S::kTask),
    C("BattleFx_PlaceOverOwner", 0x434730, 0x139, nullptr, 0, BE2_FN(BattleFx_PlaceOverOwner), S::kTask),
    C("BattleFx_NextStatusIcon", 0x434870, 0x6D, nullptr, 0, BE2_FN(BattleFx_NextStatusIcon), S::kTask, 0xFF),
    C("Battle_BackupFlagged", 0x433D60, 0x40, nullptr, 0, BE2_FN(Battle_BackupFlagged), S::kHelper, 0xFF),
    C("BattleFx_RestoreParty", 0x433DA0, 0x562, kCalls433DA0, BE2_N(kCalls433DA0), BE2_FN(BattleFx_RestoreParty), S::kTask),
    C("BattleFx_RestoreMemberTask", 0x434310, 0x26, nullptr, 0, BE2_FN(BattleFx_RestoreMemberTask), S::kTask, 0, 1, 2,
      kImms434310, BE2_N(kImms434310)),
    C("BattleFx_RestoreMember", 0x434340, 0x37B, kCalls434340, BE2_N(kCalls434340), BE2_FN(BattleFx_RestoreMember), S::kTask),
    C("BattleFx_RestoreFade", 0x4346C0, 0x6D, kCalls4346C0, BE2_N(kCalls4346C0), BE2_FN(BattleFx_RestoreFade), S::kTask),
    C("BattleFx_GridMark", 0x4348E0, 0x42, nullptr, 0, BE2_FN(BattleFx_GridMark), S::kTask, 0, 1, 3, kImms4348E0,
      BE2_N(kImms4348E0)),
    C("BattleFx_GridMarkStart", 0x434930, 0xD9, kCalls434930, BE2_N(kCalls434930), BE2_FN(BattleFx_GridMarkStart), S::kTask),
    C("BattleFx_GridMarkRun", 0x434A10, 0x50, kCalls434A10, BE2_N(kCalls434A10), BE2_FN(BattleFx_GridMarkRun), S::kTask, 0, 2,
      2, kImms434A10, BE2_N(kImms434A10)),
    C("BattleFx_GridMarkCount", 0x434A60, 0xA3, nullptr, 0, BE2_FN(BattleFx_GridMarkCount), S::kTask),
    C("BattleFx_GridMarkPlace", 0x434B10, 0x71, nullptr, 0, BE2_FN(BattleFx_GridMarkPlace), S::kTask),
    C("BattleFx_ListHand", 0x434B90, 0x42, nullptr, 0, BE2_FN(BattleFx_ListHand), S::kTask, 0, 1, 3, kImms434B90,
      BE2_N(kImms434B90)),
    C("BattleFx_ListHandStart", 0x434BE0, 0xA0, kCalls434BE0, BE2_N(kCalls434BE0), BE2_FN(BattleFx_ListHandStart), S::kTask),
    C("BattleFx_ListHandRun", 0x434C80, 0x71, kCalls434C80, BE2_N(kCalls434C80), BE2_FN(BattleFx_ListHandRun), S::kTask, 0, 2,
      2, kImms434C80, BE2_N(kImms434C80)),
    C("BattleFx_ListHandPress", 0x434D00, 0x26, kCalls434D00, BE2_N(kCalls434D00), BE2_FN(BattleFx_ListHandPress), S::kTask),
    C("BattleFx_ListHandWait", 0x434D30, 0x31, nullptr, 0, BE2_FN(BattleFx_ListHandWait), S::kTask),
    C("BattleFx_Win18Cursor", 0x434D70, 0x42, nullptr, 0, BE2_FN(BattleFx_Win18Cursor), S::kTask, 0, 1, 3, kImms434D70,
      BE2_N(kImms434D70)),
    C("BattleFx_Win18CursorStart", 0x434DC0, 0xBF, kCalls434DC0, BE2_N(kCalls434DC0), BE2_FN(BattleFx_Win18CursorStart),
      S::kTask),
    C("BattleFx_Win18CursorRun", 0x434E80, 0xB4, kCalls434E80, BE2_N(kCalls434E80), BE2_FN(BattleFx_Win18CursorRun), S::kTask),
    C("BattleFx_Win19Cursor", 0x434F40, 0x42, nullptr, 0, BE2_FN(BattleFx_Win19Cursor), S::kTask, 0, 1, 3, kImms434F40,
      BE2_N(kImms434F40)),
    C("BattleFx_Win19CursorStart", 0x434F90, 0xBF, kCalls434F90, BE2_N(kCalls434F90), BE2_FN(BattleFx_Win19CursorStart),
      S::kTask),
    C("BattleFx_Win19CursorRun", 0x435050, 0xB4, kCalls435050, BE2_N(kCalls435050), BE2_FN(BattleFx_Win19CursorRun), S::kTask),
};

// --- the action's begin
constexpr bh::CallSite kCalls435AB0[] = {{0x16, 0x5B93D2},  {0xB1, 0x5B93D2},  {0xEC, 0x452DD0},
                                         {0xF9, 0x435CF0},  {0x115, 0x452DD0}, {0x122, 0x435C40}};
constexpr bh::JumpTable kTables435AB0[] = {{0x63, 0x178, 4}};
constexpr bh::CallSite kCalls435C40[] = {{0x0, 0x5B93D2}, {0x1C, 0x435C80}, {0x2A, 0x435EF0}};
constexpr bh::CallSite kCalls435C80[] = {{0x14, 0x4456C0}, {0x49, 0x5B93D2}};
constexpr bh::CallSite kCalls435CF0[] = {{0x8A, 0x435E70}, {0xC3, 0x435E90}, {0xCD, 0x435E10},
                                         {0xF2, 0x436070}, {0xFE, 0x435EF0}, {0x10A, 0x435E90}};
constexpr bh::CallSite kCalls435E10[] = {{0x20, 0x4456C0}};
constexpr bh::CallSite kCalls435E70[] = {{0x10, 0x435C80}, {0x19, 0x435EF0}};
constexpr bh::CallSite kCalls435E90[] = {{0x1F, 0x4456C0}};
constexpr bh::CallSite kCalls435EF0[] = {{0x34, 0x4456C0}, {0x6D, 0x5B93D2}, {0x8C, 0x4456C0},
                                         {0xDD, 0x4456C0}, {0x115, 0x5B93D2}, {0x134, 0x4456C0}};
constexpr bh::CallSite kCalls436070[] = {{0x10, 0x435C80}};

const bh::Clone kBegin[] = {
    C("BattleEnemy_PickAction", 0x435AB0, 0x188, kCalls435AB0, BE2_N(kCalls435AB0), BE2_FN(BattleEnemy_PickAction), S::kHelper,
      0, 1, 0, nullptr, 0, kTables435AB0, BE2_N(kTables435AB0)),
    C("BattleEnemy_PickAnyTarget", 0x435C40, 0x35, kCalls435C40, BE2_N(kCalls435C40), BE2_FN(BattleEnemy_PickAnyTarget),
      S::kHelper, 0xFF),
    C("Battle_RandomEnemy", 0x435C80, 0x64, kCalls435C80, BE2_N(kCalls435C80), BE2_FN(Battle_RandomEnemy), S::kHelper, 0xFF),
    C("BattleEnemy_PickTarget", 0x435CF0, 0x114, kCalls435CF0, BE2_N(kCalls435CF0), BE2_FN(BattleEnemy_PickTarget),
      S::kHelper, 0xFF),
    C("Battle_EnemyLowestHp", 0x435E10, 0x56, kCalls435E10, BE2_N(kCalls435E10), BE2_FN(Battle_EnemyLowestHp), S::kHelper, 0xFF),
    C("BattleEnemy_OtherOrMember", 0x435E70, 0x1E, kCalls435E70, BE2_N(kCalls435E70), BE2_FN(BattleEnemy_OtherOrMember),
      S::kHelper, 0xFF),
    C("Battle_MemberLowestHp", 0x435E90, 0x55, kCalls435E90, BE2_N(kCalls435E90), BE2_FN(Battle_MemberLowestHp), S::kHelper,
      0xFF),
    C("Battle_RandomMember", 0x435EF0, 0x179, kCalls435EF0, BE2_N(kCalls435EF0), BE2_FN(Battle_RandomMember), S::kHelper, 0xFF),
    C("BattleEnemy_OtherEnemy", 0x436070, 0x1C, kCalls436070, BE2_N(kCalls436070), BE2_FN(BattleEnemy_OtherEnemy), S::kHelper,
      0xFF),
};

// --- the enemy ops
constexpr bh::CallSite kCalls436290[] = {{0xA, 0x4358D0}, {0x45, 0x446770}, {0x8A, 0x446770}};
constexpr bh::CallSite kCalls436330[] = {{0x0, 0x436090}, {0x32, 0x5720C0}};
constexpr bh::CallSite kCalls4365D0[] = {{0x2, 0x4358D0}};
constexpr bh::CallSite kCalls436640[] = {{0x0, 0x436090}, {0x37, 0x437450}, {0x58, 0x587740}};
constexpr bh::CallSite kCalls4366B0[] = {{0x0, 0x436090}, {0xF, 0x4530D0}, {0x17, 0x4376F0}, {0x23, 0x4376A0}};
constexpr bh::CallSite kCalls436BE0[] = {{0x2C, 0x446770}, {0x3A, 0x436090}, {0x44, 0x587740}};
constexpr bh::CallSite kCalls436C40[] = {{0x4A, 0x436090}};
constexpr bh::CallSite kCalls436C90[] = {{0x35, 0x453DA0}, {0x49, 0x435180}, {0xAB, 0x436090}};
constexpr bh::CallSite kCalls436D50[] = {{0x0, 0x4360C0}, {0x26, 0x4358D0}, {0x31, 0x4358D0}};
constexpr bh::CallSite kCalls436F20[] = {{0x41, 0x587740}, {0x98, 0x453DA0}, {0xA0, 0x436090}};
constexpr bh::CallSite kCalls436FD0[] = {{0x30, 0x446FD0}};

const bh::Clone kOps[] = {
    C("EnemyOp_SlideDispatch", 0x436270, 0x12, nullptr, 0, BE2_FN(EnemyOp_SlideDispatch), S::kDispatch, 0, 3, 2),
    C("EnemyOp_SlideStart", 0x436290, 0x9B, kCalls436290, BE2_N(kCalls436290), BE2_FN(EnemyOp_SlideStart), S::kState),
    C("EnemyOp_SlideStep", 0x436330, 0x76, kCalls436330, BE2_N(kCalls436330), BE2_FN(EnemyOp_SlideStep), S::kState),
    C("EnemyOp_TurnStart", 0x4365D0, 0x4A, kCalls4365D0, BE2_N(kCalls4365D0), BE2_FN(EnemyOp_TurnStart), S::kState),
    C("EnemyOp_CueDispatch", 0x436620, 0x12, nullptr, 0, BE2_FN(EnemyOp_CueDispatch), S::kDispatch, 0, 2, 2),
    C("EnemyOp_PlayCreatureCue", 0x436640, 0x69, kCalls436640, BE2_N(kCalls436640), BE2_FN(EnemyOp_PlayCreatureCue), S::kState),
    C("EnemyOp_CueEnd", 0x4366B0, 0x29, kCalls4366B0, BE2_N(kCalls4366B0), BE2_FN(EnemyOp_CueEnd), S::kState, 0xFF),
    C("EnemyOp_Act3Dispatch", 0x436BC0, 0x12, nullptr, 0, BE2_FN(EnemyOp_Act3Dispatch), S::kDispatch, 0, 3, 5),
    C("EnemyOp_KnockStart", 0x436BE0, 0x55, kCalls436BE0, BE2_N(kCalls436BE0), BE2_FN(EnemyOp_KnockStart), S::kState),
    C("EnemyOp_KnockBack", 0x436C40, 0x4F, kCalls436C40, BE2_N(kCalls436C40), BE2_FN(EnemyOp_KnockBack), S::kState, 0xFF),
    C("EnemyOp_KnockReturn", 0x436C90, 0xB2, kCalls436C90, BE2_N(kCalls436C90), BE2_FN(EnemyOp_KnockReturn), S::kState),
    C("EnemyOp_KnockPose", 0x436D50, 0x38, kCalls436D50, BE2_N(kCalls436D50), BE2_FN(EnemyOp_KnockPose), S::kState),
    C("EnemyOp_Act5Dispatch", 0x436F00, 0x12, nullptr, 0, BE2_FN(EnemyOp_Act5Dispatch), S::kDispatch, 0, 3, 3),
    C("EnemyOp_ApplyHpChange", 0x436F20, 0xAE, kCalls436F20, BE2_N(kCalls436F20), BE2_FN(EnemyOp_ApplyHpChange), S::kState),
    C("EnemyOp_Act5End", 0x436FD0, 0x56, kCalls436FD0, BE2_N(kCalls436FD0), BE2_FN(EnemyOp_Act5End), S::kState),
};

// The four EnemyOp tables the dispatchers jump through, counted from the
// entries' own steps (docs/battle_e2.md section 5): EnterSubs2 by +3 (the
// slide's start and step), Step5Subs by +2 (the cue and its end), Act3Subs by
// +3 (the knock-back's four steps and EnemyOp_HitEnd), Act5Subs by +3 (the HP
// change, EnemyOp_WaitAnimOnce, the end).
const bh::DataTable kOpTables[] = {{0x64B1E4, 2}, {0x64B1F4, 2}, {0x64B220, 5}, {0x64B244, 3}};

// ===========================================================================
// The callees the standard set lacks or records too coarsely
// ===========================================================================

// The per-round choices the effects read (set by Seed before both passes).
unsigned g_in = 0xFFFF;         // an actor Battle_ActorIsOut rules in, or none
const bh::Clone* g_cur = nullptr;
unsigned g_k = 0;

// Rand: the CRT's answer is 15 bits (0x5B93D2: the seed's bits 16..30); the
// standard recorder's garbage a quarter of the time would be a negative
// remainder the two picks index their frames by.
U RandRange(const U*, U answer) { return answer & 0x7FFF; }
// Battle_ActorIsOut: the round's chosen actor ruled in, so the two weighted
// picks never divide by an empty count (the original would fault on both
// passes); every other actor as the standard recorder answers.
U ActorInEffect(const U* a, U answer) { return (a[0] & 0xFF) == g_in ? answer & 0xFFFFFF00u : answer; }
// 0x446770 (BE4's): turns the sprite's velocity pair by its facing +8 as the
// real one does (1: (-y, x), 2: (-x, -y), 3: (y, -x)) - the slide and the
// knock-back read the pair after the call. Only inside the enemies' objects,
// the task slots, the party and the harness's records.
U TurnEffect(const U* a, U answer) {
    const U p = a[0];
    const bool enemy = p >= 0x93A000 && p + 0x14 <= 0x93C8A0;
    const bool member = p >= 0x802D40 && p + 0x14 <= 0x802D40 + 3 * 0x14C;
    const bool record = (p >= Key(bh::SpriteRecord(0)) && p + 0x14 <= Key(bh::SpriteRecord(0)) + 0x140) ||
                        (p >= Key(bh::SpriteRecord(1)) && p + 0x14 <= Key(bh::SpriteRecord(1)) + 0x140);
    if (!enemy && !member && !record) return answer;
    auto* s = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(p));
    const U x = static_cast<U>(move_script::Long(s + 0xC)), y = static_cast<U>(move_script::Long(s + 0x10));
    switch (s[8]) {
    case 1: SetLong(s + 0xC, static_cast<std::int32_t>(0u - y)); SetLong(s + 0x10, static_cast<std::int32_t>(x)); break;
    case 2: SetLong(s + 0xC, static_cast<std::int32_t>(0u - x)); SetLong(s + 0x10, static_cast<std::int32_t>(0u - y)); break;
    case 3: SetLong(s + 0xC, static_cast<std::int32_t>(y)); SetLong(s + 0x10, static_cast<std::int32_t>(0u - x)); break;
    default: break;
    }
    return answer;
}

const bh::Callee kCallees[] = {
    {"Rand", 0x5B93D2, KeyOf(&::Rand), 0, {}, A::kRand, 0, 0, {}, &RandRange},
    {"Battle_ActorIsOut", bof3::addr::Battle_ActorIsOut, KeyOf(&::Battle_ActorIsOut), 1, {kU8}, A::kFlag, 0, 0, {},
     &ActorInEffect},
    // the standard lists it whole; every caller here pushes a register whose
    // upper bytes are its own (0x452DD0 reads the low byte, its disassembly)
    {"0x452DD0", at::kActorMayAct, at::kActorMayAct, 1, {kU8}, A::kFlag, 0, 0},
    // the actor word likewise: al loaded over a pointer's upper bytes
    {"Battle_SetDamagePopup", bof3::addr::Battle_SetDamagePopup, KeyOf(&::Battle_SetDamagePopup), 2, {kU16, kU8},
     A::kGarbage, 0, 0},
    // other groups' (battle_e2_callees.h), raw until they merge
    {"0x442310", at::kMemberRecalc, at::kMemberRecalc, 0, {0x905D98}, A::kPhase, 0, 0},   // logs Field_State
    {"0x453300", at::kMemberStatusSet, at::kMemberStatusSet, 1, {kU8}, A::kGarbage, 0, 0},
    {"0x446770", at::kTurnVelocity, at::kTurnVelocity, 1, {kAll}, A::kGarbage, 0, 0, {}, &TurnEffect},
    {"0x437450", at::kPlayCue, at::kPlayCue, 1, {kU16}, A::kGarbage, 0, 0},
    {"0x4376F0", at::kEnemyTaskChance, at::kEnemyTaskChance, 0, {}, A::kPhase, 0, 0},
    {"0x4376A0", at::kEnemyOpEnd, at::kEnemyOpEnd, 0, {}, A::kPhase, 0, 0},
    // the group's own, called directly
    {"BattleFx_NextStatusIcon", 0x434870, KeyOf(&::BattleFx_NextStatusIcon), 0, {}, A::kByte, 0xFF, 0x0F},
    {"BattleFx_PlaceOverOwner", 0x434730, KeyOf(&::BattleFx_PlaceOverOwner), 0, {}, A::kPhase, 0, 0},
    {"BattleEnemy_PickTarget", 0x435CF0, KeyOf(&::BattleEnemy_PickTarget), 1, {kU8}, A::kByte, 0xFF, 0x0A},
    {"BattleEnemy_PickAnyTarget", 0x435C40, KeyOf(&::BattleEnemy_PickAnyTarget), 1, {kU8}, A::kByte, 0xFF, 0x0A},
    {"Battle_RandomEnemy", 0x435C80, KeyOf(&::Battle_RandomEnemy), 1, {kU8}, A::kByte, 3, 10},
    {"Battle_RandomMember", 0x435EF0, KeyOf(&::Battle_RandomMember), 0, {}, A::kByte, 0, 3},
    {"Battle_EnemyLowestHp", 0x435E10, KeyOf(&::Battle_EnemyLowestHp), 0, {}, A::kByte, 0, 10},
    {"Battle_MemberLowestHp", 0x435E90, KeyOf(&::Battle_MemberLowestHp), 0, {}, A::kByte, 0, 2},
    {"BattleEnemy_OtherOrMember", 0x435E70, KeyOf(&::BattleEnemy_OtherOrMember), 1, {kU8}, A::kByte, 0, 10},
    {"BattleEnemy_OtherEnemy", 0x436070, KeyOf(&::BattleEnemy_OtherEnemy), 1, {kU8}, A::kByte, 0xFF, 0x0A},
};

// The cells beyond the engine frame that the group reads or writes.
const bh::Region kRegions[] = {
    {0x939B20, 0x3A0},       // the backup party records 0x939AE0 past the standard 0x939AD0..0x939B20, to the engine's 0x939EC0
    {at::kAnimSet, 4},       // the pose pool pointer 0x9039D8
    {at::kKeptAp, 0xC},      // the kept cells 0x675ECC..0x675ED7
    {at::kFormation, 4},     // the formation 0x904060
    {at::kMemberPositions, 0x18},   // the members' spots 0x7E06E0
};

// ===========================================================================
// Seeds, arguments, disturbance
// ===========================================================================

unsigned char Byte(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<unsigned char>(bh::Next());
    const U* v = often.begin();
    return static_cast<unsigned char>(v[bh::Next() % often.size()]);
}
unsigned char* Owner() { return bh::Pointer(at::kOwner); }
unsigned char* Backup(unsigned i) { return Mem(at::kBackup + (i % 3) * at::kPartyStride); }
unsigned char* Enemy() { return bh::Pointer(at::kEnemyCurrent); }

// The formations whose weights (both tables) are all non-zero and sum below
// 0x100, found at start-up in the loaded image: with one actor ruled in, the
// weighted pick's byte sum is never 0 (docs/battle_e2.md section 5).
unsigned g_good[32];
unsigned g_good_n = 0;
void FindFormations() {
    g_good_n = 0;
    for (unsigned f = 0; f < 32; ++f) {
        const unsigned char* const w3 = Mem(at::kFormationWeights3 + 3 * f);
        const unsigned char* const w2 = Mem(at::kFormationWeights2 + 2 * f);
        if (w3[0] == 0 || w3[1] == 0 || w3[2] == 0 || w2[0] == 0 || w2[1] == 0) continue;
        if (w3[0] + w3[1] + w3[2] >= 0x100 || w2[0] + w2[1] >= 0x100) continue;
        g_good[g_good_n++] = f;
    }
    if (g_good_n == 0) bof3::Fatal("battle_e2 fuzz: no formation with non-zero weights in 0x64B184 / 0x64B18C");
}

// Ability ids by the branch BattleEnemy_PickTarget takes on their flag byte
// (NameTable_Abilities +0, read in the loaded image at start-up): 0x10 set; 0x10
// and 0x40 clear; 0x40 with 0x20; 0x40 without 0x20. Each class drawn alike.
unsigned short g_ids[4][0x400];
unsigned g_ids_n[4];
void FindAbilities() {
    for (unsigned& n : g_ids_n) n = 0;
    for (unsigned id = 0; id < 0x400; ++id) {
        const unsigned char a = Mem(at::kAbilities + id * 24)[0];
        const unsigned c = (a & 0x10) != 0 ? 0 : (a & 0x40) == 0 ? 1 : (a & 0x20) != 0 ? 2 : 3;
        g_ids[c][g_ids_n[c]++] = static_cast<unsigned short>(id);
    }
}
unsigned AbilityId() {
    const unsigned c = bh::Next() % 4;
    if (g_ids_n[c] == 0) return bh::Next() % 0x400;
    return g_ids[c][bh::Next() % g_ids_n[c]];
}

// The owner of a watch or marker task: a party member or an enemy with its
// own actor byte most of the time, the harness's record otherwise, and the
// actor 0..10 always (an enemy's index reads past .data above 25).
void SeedOwner(bool member_only) {
    const unsigned pick = bh::Next() % 4;
    unsigned char* o = Owner();
    if (member_only || pick < 2) {
        const unsigned m = bh::Next() % 3;
        o = bh::PartyOf(static_cast<unsigned char>(m));
        bh::SetPointer(at::kOwner, o);
        o[5] = static_cast<unsigned char>(bh::Often() ? m : bh::Next() % 3);
    } else if (pick == 2) {
        const unsigned e = bh::Next() % 8;
        o = bh::EnemyAt(e);
        bh::SetPointer(at::kOwner, o);
        o[5] = static_cast<unsigned char>(bh::Often() ? 3 + e : 3 + bh::Next() % 8);
    }
    if (member_only) o[5] = static_cast<unsigned char>(o[5] % 3);
    else if (o[5] > 10) o[5] = static_cast<unsigned char>(o[5] % 11);
}

// A member's record and its backup with non-zero maxima (the restore divides
// by +0xA0 and +0xA2 after the copy), characters 0 and 7 often.
void SeedRestore() {
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const m = bh::PartyOf(static_cast<unsigned char>(i));
        unsigned char* const b = Backup(i);
        for (unsigned char* r : {m, b}) {
            if (Word(r + 0xA0) == 0) SetWord(r + 0xA0, 1 + bh::Next() % 999);
            if (Word(r + 0xA2) == 0) SetWord(r + 0xA2, 1 + bh::Next() % 99);
            r[0x89] = Byte({0, 7, 1, 4});
            r[0x148] = static_cast<unsigned char>(bh::Next() % 3);
        }
        // the copy brings the backup's actor byte into the owner (the member
        // itself in the game): 0..2 there too
        b[5] = static_cast<unsigned char>(bh::Often() ? i : bh::Next() % 3);
    }
    Mem(at::kSavedCount)[0] = static_cast<unsigned char>(bh::Next() % 5);
    Mem(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(bh::Half() ? Mem(at::kRoundFlags + 1)[0] | 0x80 : Mem(at::kRoundFlags + 1)[0] & 0x7F);
    SeedOwner(true);
    Sprite_Current[5] = static_cast<unsigned char>(Sprite_Current[5] % 3);
}

// Window record 21 / 18 / 19 in use half the time; the grid cell +0xB and the
// pick list around it; the cursors' row bases and their copies.
void SeedMarkers() {
    unsigned char* const s = Sprite_Current;
    for (U w : {at::kWindow18, at::kWindow19, at::kWindow21}) {
        unsigned char* const r = Mem(w);
        r[0] = static_cast<unsigned char>(bh::Half() ? r[0] | 1 : r[0] & 0xFE);
        SetWord(r + 0x12, static_cast<unsigned>(static_cast<std::int32_t>(bh::Next() % 5) - 1));
        SetWord(r + 0x1E, bh::Half() ? Word(r + 0x12) : Word(r + 0x12) + 1);
    }
    s[0xB] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 18 : bh::Next());
    s[9] = static_cast<unsigned char>(bh::Next() % 4);
    s[0xA] = static_cast<unsigned char>(bh::Next() % 6);
    Mem(at::kPickCount)[0] = Byte({0, 1, 2, 3});
    for (unsigned i = 0; i < 3; ++i)
        Mem(at::kPickList + i)[0] = static_cast<unsigned char>(bh::Half() ? s[0xB] : bh::Next() % 18);
    Mem(at::kTapCommand)[0] = Byte({0xFF, 0xFF, 0, 1, 4});
    for (U g : {at::kGrid18, at::kGrid19})
        for (unsigned i = 0; i < 0x48; ++i)
            if (bh::Next() % 4 == 0) Mem(g + i)[0] = 0xFF;
    s[2] = static_cast<unsigned char>(s[2] % 3);
}

// The enemy of an op: HP against the change, the flags the ops test, the
// velocity and the destination at their meeting point half the time.
void SeedOp(U base) {
    unsigned char* const s = Sprite_Current;
    unsigned char* const e = Enemy();
    s[8] = Byte({0, 1, 2, 3, 4});
    s[0xA] = Byte({1, 1, 2, 0, 4});
    s[9] = Byte({1, 1, 2, 0});
    e[0xF0] = static_cast<unsigned char>(bh::Next() % 8);
    if (bh::Half()) Mem(at::kFight)[0] = 0;
    // a dispatcher's other state bytes inside its table (the harness drew its
    // own): a plant reading the wrong byte lands on another entry, a count
    switch (base) {
    case 0x436270: bh::OtherStates(3, 2); break;
    case 0x436620: bh::OtherStates(2, 2); break;
    case 0x436BC0: bh::OtherStates(3, 5); break;
    case 0x436F00: bh::OtherStates(3, 3); break;
    default: break;
    }
    if (base == 0x436330 && bh::Half()) {
        SetLong(s + 0x18, move_script::Long(s + 0x34) + move_script::Long(s + 0xC));
        SetLong(s + 0x1C, move_script::Long(s + 0x38) + move_script::Long(s + 0x10));
    }
    if (base == 0x436F20) {
        const unsigned hp = bh::Often() ? BH_PICK(0, 1, 2, 0x10, 0x7FFF, 0x8000, 0xFFFF) : bh::Next() & 0xFFFF;
        SetWord(e + 0xA4, hp);
        const U changes[] = {0, 1, 0xFFFF, hp, hp - 1, hp + 1, 0x7FFF, 0x8000, 0xFFF0};
        SetWord(e + 0x108, bh::Often() ? changes[bh::Next() % 9] : bh::Next());
        const unsigned after = (hp - Word(e + 0x108)) & 0xFFFF;
        SetWord(e + 0xB0, bh::Often() ? (after + (bh::Next() % 3) - 1) & 0xFFFF : bh::Next());
    }
}

void Seed(unsigned k) {
    g_k = k;
    g_in = 0xFFFF;
    const U base = g_cur[k].base;
    unsigned char* const s = Sprite_Current;
    switch (base) {
    // --- the watch and its helpers
    case 0x433650:
        SeedOwner(false);
        Mem(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(bh::Half() ? Mem(at::kRoundFlags + 1)[0] | 4 : Mem(at::kRoundFlags + 1)[0] & 0xFB);
        if (bh::Half()) Mem(at::kActor)[0] = Owner()[5];
        Mem(at::kActKind)[0] = Byte({4, 4, 1});
        SetWord(Mem(at::kMagicId), bh::Often() ? BH_PICK(0x97, 0xE1, 0x96, 0xE0) : bh::Next());
        s[0xA] = Byte({0, 0, 1, 2});
        s[0xB] = static_cast<unsigned char>(bh::Next() % 16);
        break;
    case 0x434730:
        SeedOwner(false);
        Owner()[8] = static_cast<unsigned char>(bh::Next() % 4);
        for (unsigned i = 0; i < 3; ++i) bh::PartyOf(static_cast<unsigned char>(i))[0x89] = static_cast<unsigned char>(bh::Next() % 8);
        for (unsigned i = 0; i < 8; ++i) bh::EnemyAt(i)[0xF0] = static_cast<unsigned char>(bh::Next() % 8);
        break;
    case 0x434870: {
        SeedOwner(false);
        s[0xB] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 16 : 16 + bh::Next() % 240);
        const unsigned a = Owner()[5];
        unsigned char* const st = a <= 2 ? bh::PartyOf(static_cast<unsigned char>(a)) + 0x90 : bh::EnemyAt(a - 3) + 0x92;
        // +0xB of 16 or more with no bit spins the original for ever: a bit then
        if (s[0xB] >= 16) {
            if ((st[0] & 0x58) == 0) st[0] = static_cast<unsigned char>(st[0] | 0x10);
        } else if (bh::Half()) {
            st[0] = static_cast<unsigned char>(st[0] & ~0x58);
        }
        break;
    }
    // --- the restore
    case 0x433D60:
        for (unsigned i = 0; i < 3; ++i) {
            Backup(i)[0] = static_cast<unsigned char>(bh::Often() ? Backup(i)[0] | 1 : Backup(i)[0] & 0xFE);
            Backup(i)[0x134] = static_cast<unsigned char>(bh::Half() ? Backup(i)[0x134] | 1 : Backup(i)[0x134] & 0xFE);
        }
        break;
    case 0x433DA0: case 0x434310: case 0x434340:
        SeedRestore();
        break;
    case 0x4346C0:
        SeedOwner(true);
        SetLong(Owner() + 0x40, static_cast<std::int32_t>(bh::Often() ? BH_PICK(0x10000, 0x10000, 0xE000, 0x12000, 0) : bh::Next()));
        break;
    // --- the markers
    case 0x4348E0: case 0x434930: case 0x434A10: case 0x434A60: case 0x434B10: case 0x434B90: case 0x434BE0:
    case 0x434C80: case 0x434D00: case 0x434D30: case 0x434D70: case 0x434DC0: case 0x434E80: case 0x434F40:
    case 0x434F90: case 0x435050:
        SeedMarkers();
        break;
    // --- the action's begin
    case 0x435AB0: {
        // every enemy's fields: the argument picks one
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const x = bh::EnemyAt(i);
            SetWord(x + 0xBA, Byte({0, 1, 2, 0xFF}));
            x[0x92] = static_cast<unsigned char>(bh::Next() % 4 == 0 ? x[0x92] | 0x20 : x[0x92] & 0xDF);
            x[0x115] = static_cast<unsigned char>(bh::Next() % 4 == 0 ? x[0x115] | 0x40 : x[0x115] & 0xBF);
        }
        Mem(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(bh::Half() ? Mem(at::kRoundFlags + 1)[0] | 0x40 : Mem(at::kRoundFlags + 1)[0] & 0xBF);
        if (bh::Half()) Mem(at::kFight)[0] = 0;
        Mem(at::kTarget)[0] = Byte({0xFF, 0xFF, 3, 0});
        break;
    }
    case 0x435C40:
        Mem(at::kEnemiesLeft)[0] = Byte({1, 1, 2, 3, 0});
        bh::SetRandHint(3);
        break;
    case 0x435C80:
        g_in = 3 + bh::Next() % 8;
        break;
    case 0x435CF0:
        Mem(at::kActKind)[0] = Byte({1, 1, 4, 4, 4, 0, 2, 3, 5});
        for (unsigned i = 0; i < 8; ++i) SetWord(bh::EnemyAt(i) + 0x106, bh::Often() ? AbilityId() : bh::Next());
        break;
    case 0x435E10:
        for (unsigned i = 0; i < 8; ++i)
            SetWord(bh::EnemyAt(i) + 0xA4, bh::Often() ? BH_PICK(0, 1, 2, 0xFFFE, 0xFFFF, 0x10) : bh::Next());
        break;
    case 0x435E90:
        for (unsigned i = 0; i < 3; ++i)
            SetWord(bh::PartyOf(static_cast<unsigned char>(i)) + 0x98,
                    bh::Often() ? BH_PICK(0, 1, 0x270F, 0x2710, 0x2711, 0x10) : bh::Next());
        break;
    case 0x435E70: case 0x436070:
        Mem(at::kEnemiesLeft)[0] = Byte({1, 1, 2, 3, 0});
        break;
    case 0x435EF0:
        SetLong(Mem(at::kPartyCount), static_cast<std::int32_t>((bh::Next() & 0xFFFFFF00u) | Byte({3, 3, 2, 2, 1, 0, 4})));
        SetLong(Mem(at::kFormation), static_cast<std::int32_t>((bh::Next() & 0xFFFFFF00u) | g_good[bh::Next() % g_good_n]));
        g_in = bh::Next() % (Mem(at::kPartyCount)[0] == 2 ? 2 : 3);
        break;
    default:
        SeedOp(base);
        break;
    }
}

// The words: an enemy index 0..7 (the originals index the eight objects by
// its low byte) with garbage above it half the time; Battle_RandomEnemy's
// actor to leave out, never the round's chosen one (so a pick is left).
void Args(unsigned k, U* a) {
    const U base = g_cur[k].base;
    const U upper = bh::Half() ? bh::Next() & 0xFFFFFF00u : 0;
    if (base == 0x435C80) {
        unsigned x = bh::Often() ? 3 + bh::Next() % 8 : bh::Next() & 0xFF;
        if (x == g_in) x = x == 10 ? 3 : x + 1;
        a[0] = upper | x;
    } else {
        a[0] = upper | (bh::Next() % 8);
    }
}

// The cells the group's functions read again after a call that the standard
// disturbance leaves: the owner's actor and state, the restore's counts,
// window record 21's use, the tapped command, the enemies left.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 6) {
    case 0: Owner()[5] = b; break;
    case 1: Mem(at::kPartyCount)[0] = static_cast<unsigned char>(b % 4); break;
    case 2: Mem(at::kWindow21)[0] = b; break;
    case 3: Mem(at::kTapCommand)[0] = h & 0x1000000 ? 0xFF : b; break;
    case 4: Mem(at::kEnemiesLeft)[0] = b; break;
    default: Mem(at::kRoundFlags + 1)[0] = b; break;
    }
}

// What the functions cannot survive garbage in, put back after every
// disturbance: the actor a watch or marker reads (0..10: an enemy past 25
// reads past .data), the member a restore writes (0..2) - both the owner's
// and Sprite_Current's.
void Settle() {
    if (g_cur == nullptr) return;
    const U base = g_cur[g_k].base;
    const bool restore = base == 0x433DA0 || base == 0x434310 || base == 0x434340 || base == 0x4346C0;
    unsigned char* const o = Owner();
    if (restore) {
        o[5] = static_cast<unsigned char>(o[5] % 3);
        Sprite_Current[5] = static_cast<unsigned char>(Sprite_Current[5] % 3);
    } else if (o[5] > 10) {
        o[5] = static_cast<unsigned char>(o[5] % 11);
    }
}

// BOF3X_BE2_RUN=<tasks | begin | ops> runs that one alone (the controls
// script's shortcut); unset, all three run.
bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BE2_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

// BOF3X_BE2_ONLY=<function name> fuzzes that clone alone (a probe's and the
// controls script's shortcut).
bh::Clone g_only[1];
void RunFamily(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables) {
    if (!Wants(run)) return;
    if (const char* only = std::getenv("BOF3X_BE2_ONLY"); only != nullptr && *only != 0) {
        unsigned i = 0;
        while (i < n && std::strcmp(clones[i].name, only) != 0) ++i;
        if (i == n) return;
        g_only[0] = clones[i];
        clones = g_only;
        n = 1;
    }
    g_cur = clones;
    bh::Group g{"battle_e2", clones, n, kCallees, BE2_COUNT(kCallees), tables, n_tables, kRegions, BE2_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.settle = &Settle;
    g.engine = true;
    bh::Run(g);
    g_cur = nullptr;
    g_in = 0xFFFF;
}

}  // namespace

void SelfTest() {
    FindFormations();
    FindAbilities();
    RunFamily("tasks", kTasks, BE2_COUNT(kTasks), nullptr, 0);
    RunFamily("begin", kBegin, BE2_COUNT(kBegin), nullptr, 0);
    RunFamily("ops", kOps, BE2_COUNT(kOps), kOpTables, BE2_COUNT(kOpTables));
}

}  // namespace battle_e2
