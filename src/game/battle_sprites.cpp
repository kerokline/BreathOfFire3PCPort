// Group BG of the seventh round (docs/battle_sprites.md): 32 functions of the
// battle engine and two small helpers, each faithful to Capcom's PC code but
// one: since DIV-0185 Battle_SetHitPopup reads a null target block as bit 1
// clear instead of faulting.
//
//   0x452BF0 Battle_RollPendingFlag     0x453FA0 Battle_MemberAutoTarget
//   0x452F70 Battle_SetTargetFlags      0x454220 Battle_MemberOutAction
//   0x4530D0 Battle_SetTargetFlag40     0x454380 Battle_PlayHitSound
//   0x453190 Battle_AnyFlagF0           0x454410 Battle_SetHitPopup
//   0x453210 Battle_ActionSuitsTarget   0x454CC0 Sprite_SetTint
//   0x4532A0 Battle_ItemSuitsTarget     0x454D60 Tint_Release
//   0x4537A0 Battle_PickFlag8Member     0x454DF0 ClutMap_Mark
//   0x453A10 Battle_MemberCoinFlip      0x454F30 ClutMap_FindFree
//   0x453B10 Battle_SettleFlag8         0x455140 ClutMap_FindOwner
//   0x453C00 Formation_ApplyStatMods    0x4551A0 Sprite_SetClutStp
//   0x453DA0 Battle_SetDamagePopup
//   0x494280 Battle_InitEncounterKind   0x494A80 Battle_OpenEnemyNames
//   0x4942A0 Battle_InitBossEncounter   0x494EA0 Battle_EnemyKindFlag
//   0x4942C0 Battle_InitEnemies         0x494ED0 Battle_SetEnemyKindFlag
//   0x494320 Battle_SetupEnemy          0x494F00 Battle_SetEnemyOffset
//   0x4946C0 Battle_CopyEnemyData
//   0x588F00 Sprite_UpdateScreenSlot    0x587900 Sound_PlayById
//
// Actors: 0..2 are the party records at ObjTrio (0x802D40, 0x14C bytes each),
// 3..10 the enemy records at 0x93B960 (0x128 bytes each, index actor - 3);
// an enemy record's first 0x80 bytes are its sprite object, the battle data
// (docs/kinship-probe-battle-engine.md's EnemyWorkingRecords) starts at +0x80.
// The party's flags dword is +0x130, the enemies' +0x110.
//
// Every index is unchecked, as the originals have it; every call out goes
// through battle_sprites::g (battle_sprites_callees.h).
#include "game/battle_sprites.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_sprites_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound_sp.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

// The group's functions, declared here for the pointers below; symbols.toml
// binds each name to this file (bof3/symbols.gen.h).
namespace battle_sprites {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

template <class F> F Raw(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }

const Callees kOriginals = {
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActorIsOut)),
    Rand,
    Raw<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_SetActorBit)),
    Raw<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::AbilityList_Add)),
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Item_UseFlags)),   // group TWO's (2026-10-06): by address, the value unchanged
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::BattleTask_Create)),
    Stat_AddClamped,
    Stat_AddCap100,
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Window_Alloc)),
    Sound_PlayEffect,
    AreaMap_Elevation,
    Sprite_SetAnimationBank,
    Sprite_UpdateScreen,
    // rebound 2026-09-29 (round twelve, BE6): the values unchanged, the fuzz keys on them
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberRollByAction)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActionBitSet)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberListFull)),
    Battle_MemberCoinFlip,
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_AutoTargetCheck)),   // R4A's (round 14): by address, the value unchanged
    Raw<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberAutoFixed)),            // R4A's
    Raw<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PickEnemyTarget)),
    Battle_MemberOutAction,
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberActionIs0E)),   // R4A's
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_RandomOtherMember)),  // R4A's
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_RandomEnemy)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_DefaultTarget)),
    ClutMap_Mark,
    ClutMap_FindFree,
    ClutMap_FindOwner,
    Raw<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PlaceBossActors)),   // R3G's: by address, as the other groups' above
    Battle_InitBossEncounter,
    Battle_InitEnemies,
    Battle_SetupEnemy,
    Battle_CopyEnemyData,
    Battle_SetEnemyOffset,
    Battle_EnemyKindFlag,
};
Callees g = kOriginals;

namespace {

unsigned char* Enemy(std::uint32_t i) { return At(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }
unsigned char* Ptr(const unsigned char* at) { return At(static_cast<std::uint32_t>(Long(at))); }
std::int32_t Address(const void* p) { return static_cast<std::int32_t>(Low32(p)); }
void OrLong(unsigned char* at, std::uint32_t bits) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) | bits)); }
unsigned char* EnemyData(std::uint32_t id) { return At(::bof3::Img(at::kEnemyData.v) + id * at::kEnemyDataStride); }

}  // namespace
}  // namespace battle_sprites

using namespace battle_sprites;

// original 0x494280 (0x800A8AD4, the PSX's Battle_InitEncounterKind): a boss
// encounter (0x904AAA not 0) or an ordinary one - both tail jumps.
extern "C" void __cdecl Battle_InitEncounterKind(void) {
    if (EventBattle_Id) g.boss_encounter();
    else g.normal_encounter();
}

// original 0x4942A0 (0x800A8B10): 0x494500, then a tail jump through the
// handler table 0x656954 by the boss index (read again, unchecked). The 24
// handlers read no arguments (each sets 0x904B64..0x904B6C or returns).
// Logs the fight: the id byte 0x904AAA and the set-up it enters, so a play
// session says which Boss_SetupTable entry ran (docs/takeover-queue-round11.md
// section 4, the set-ups whose fight the code does not settle). A log line
// only; the game sees nothing of it. The battle_sprites fuzz reaches this
// about 3,000 times per self-test, so expect that many lines there.
extern "C" void __cdecl Battle_InitBossEncounter(void) {
    g.boss_common();
    const unsigned id = EventBattle_Id;
    const std::uint32_t handler = static_cast<std::uint32_t>(Long(At(::bof3::Img(Boss_SetupTable_addr) + id * 4u)));
    bof3::Log("boss encounter: id %u (0x%02X) -> set-up 0x%08X", id, id, static_cast<unsigned>(handler));
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(handler))();
}

// original 0x4942C0 (0x800A8B60): the enemy count to 0; each used record of
// the eight at 0x939F00 set up as the next enemy (Battle_SetupEnemy(n, +1,
// +4, +8)); 0x904AB3 = the count Battle_SetupEnemy left.
extern "C" void __cdecl Battle_InitEnemies(void) {
    Battle_EnemyCount = 0;
    unsigned char n = 0;
    for (unsigned r = 0; r < 8; ++r) {
        const unsigned char* const rec = At(::bof3::Img(Encounter_Slots_addr) + r * 12);
        if (!rec[0]) continue;
        g.setup_enemy(n, rec[1], Long(rec + 4), Long(rec + 8));
        ++n;
    }
    Battle_EnemiesLeft = Battle_EnemyCount;
}

// original 0x494320 (no twin found): enemy `slot` from enemy data `id` at
// (x, z): its record's sprite becomes Sprite_Current and gets byte +0 = 1
// (0x21 for the banks 0x2CD and 0x2E1), +5 = slot + 3, +0x29 = 4, +8 = the
// layout ^ 2, +0x4B = 0xFF, the position +0x34 / +0x38 and its ground +0x3E
// (AreaMap_Elevation), a cleared state, the animation bank (the data's u16
// +0x1A, also kept at +0x9A); then Battle_CopyEnemyData, Battle_SetEnemyOffset,
// +0x8F = Battle_EnemyKindFlag(the data's +0xC), and the enemy count up by
// one. Sprite_Current is read again before every store, as the original
// does. The id indexes the data by 16 bits here, by 8 in
// Battle_CopyEnemyData.
//
// DIV-0187 (src/game/cheats.cpp): under BOF3X_ENEMY_INFO=1 the byte +0x8F
// is 1 whatever the kind's bit says - the HP gauge and Identify's panel
// read it - and the bit is still read, as the original reads it.
extern "C" void __cdecl Battle_SetupEnemy(unsigned slot, unsigned id, long x, long z) {
    const std::uint32_t n = slot & 0xFF;
    unsigned char* const e = Enemy(n);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
    auto* const enemy_data = rv::As<records::EnemyData>(EnemyData(id & 0xFFFF));
    Sprite_Current = e;
    enemy_rec->flags = 1;
    if (enemy_data->bank == 0x2CD || enemy_data->bank == 0x2E1) rv::Current<records::BattleEnemy>()->flags = 0x21;
    rv::Current<records::BattleEnemy>()->state = 0;
    rv::Current<records::BattleEnemy>()->state4 = 0;
    rv::Current<records::BattleEnemy>()->state3 = 0;
    rv::Current<records::BattleEnemy>()->step = 0;
    rv::Current<records::BattleEnemy>()->ot_slot = 4;
    rv::Current<records::BattleEnemy>()->actor = static_cast<unsigned char>(slot + 3);
    Sprite_Current[0x24] = 0;
    rv::Store<std::int32_t>(rv::Current<records::BattleEnemy>()->step_y, 0);
    rv::Store<std::int32_t>(rv::Current<records::BattleEnemy>()->step_z, 0);
    rv::Store<std::int32_t>(rv::Current<records::BattleEnemy>()->step_x, 0);
    rv::Store<std::int32_t>(rv::Current<records::BattleEnemy>()->work_20, 0);
    rv::Store<std::int32_t>(rv::Current<records::BattleEnemy>()->work_1c, 0);
    rv::Store<std::int32_t>(rv::Current<records::BattleEnemy>()->work_18, 0);
    rv::Current<records::BattleEnemy>()->draw_key = 0;
    rv::Current<records::BattleEnemy>()->screen_y = 0;
    rv::Current<records::BattleEnemy>()->screen_x = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5C] = 0;
    Sprite_Current[6] = 0;
    Sprite_Current[7] = 0;
    const unsigned char layout = static_cast<unsigned char>(At(::bof3::Img(at::kLayout.v))[0] ^ 2);
    rv::Current<records::BattleEnemy>()->layout = layout;
    Sprite_Current[0x48] = 0;
    rv::Current<records::BattleEnemy>()->animation = 0xFF;
    rv::Current<records::BattleEnemy>()->x = static_cast<std::int32_t>(x);
    rv::Current<records::BattleEnemy>()->z = static_cast<std::int32_t>(z);
    unsigned char* const s = Sprite_Current;
    const long ground = g.elevation(rv::As<records::BattleEnemy>(s)->x, rv::As<records::BattleEnemy>(s)->z);
    rv::Store<unsigned>(rv::Current<records::BattleEnemy>()->height_whole, static_cast<unsigned>(ground));
    Sprite_Current[0x2B] = 0;
    g.set_bank(enemy_data->bank);
    enemy_rec->bank = enemy_data->bank;
    g.copy_enemy(slot, id);
    g.enemy_offset(slot, id);
    const unsigned char seen = g.kind_flag(enemy_data->kind);
    enemy_rec->kind_flag = Cheats_EnemyInfoShown() ? 1 : seen;
    Battle_EnemyCount = static_cast<unsigned char>(Battle_EnemyCount + 1);
}

// original 0x4946C0 (0x800A9148): the enemy data `id` (by its low byte) into
// enemy `slot`'s working record (+0x80 on, EnemyWorkingRecords): the 12-byte
// NAME from data +0 to record +0x80 (0x93B9E0 + slot * 0x128), then the data's
// fields (docs/battle_sprites.md section 3), +0xF0 = the id's low byte, and
// the battle state cleared. The stat block copied to +0xB0.. is copied again
// to +0x90.. (the original's rep movsd), which includes the resistance bytes.
//
// DIV-0082: the AI row-done byte +0xF1 (record +0x71) is cleared here too.
// The original never clears it: on the PlayStation the enemy objects sit
// inside BATTLE.EMI#3's image and come back zeroed from disc at every battle
// entry, so a once-only AI row (the Volt's EXP x3 on a thunder hit) can fire
// in every fight; the port keeps the objects in static memory, and a row
// marked done in an earlier fight stays done for whoever spawns into the
// slot next (docs/trigger-mode-enemies.md).
extern "C" void __cdecl Battle_CopyEnemyData(unsigned slot, unsigned id) {
    const unsigned char* const src = EnemyData(id & 0xFF);
    auto* const from_rec = rv::AsConst<records::EnemyData>(src);
    unsigned char* const dst = Enemy(slot & 0xFF) + 0x80;
    auto* const to_rec = rv::As<records::BattleEnemy>(dst - 0x80);
    SetLong(dst, Long(src));
    SetLong(dst + 4, Long(src + 4));
    SetLong(dst + 8, Long(src + 8));
    to_rec->enemy_kind = from_rec->kind;
    to_rec->family = from_rec->family;
    to_rec->odds_index = from_rec->odds_index;
    to_rec->flags_90 = from_rec->flags;
    to_rec->zenny = from_rec->zenny;
    to_rec->exp = from_rec->exp;
    to_rec->level = from_rec->level;
    SetLong(dst + 0x1C, Long(src + 0x1C));
    SetLong(dst + 0x20, Long(src + 0x20));
    to_rec->status = from_rec->status;
    SetLong(dst + 0x28, Long(src + 0x30));
    SetLong(dst + 0x2C, Long(src + 0x34));
    SetWord(dst + 0x50, from_rec->hp);
    to_rec->hp = from_rec->hp;
    SetWord(dst + 0x52, from_rec->ap);
    to_rec->ap = from_rec->ap;
    rv::Store<unsigned>(to_rec->base_atk, from_rec->stats[0]);
    rv::Store<unsigned>(to_rec->base_def, from_rec->stats[1]);
    rv::Store<unsigned>(to_rec->base_agi, from_rec->stats[2]);
    rv::Store<unsigned>(to_rec->base_intel, from_rec->stats[3]);
    for (unsigned k = 0; k < 9; ++k) dst[0x5F + k] = src[0x7C + k];
    for (unsigned k = 0; k < 32; ++k) dst[0x30 + k] = dst[0x50 + k];
    to_rec->run_state = from_rec->run_state;
    to_rec->hit_class = from_rec->hit_class;
    to_rec->data_id = static_cast<unsigned char>(id);
    to_rec->ai_rows_done = 0;  // DIV-0082: the AI rows' done bits, which the original leaves from the last fight
    to_rec->counter = 0;
    to_rec->flags_114 = 0;
    to_rec->flags_110 = 0;
    to_rec->count_4000 = 0;
    dst[0xA3] = 0;
    to_rec->count_40 = 0;
    to_rec->count_80 = 0;
    SetLong(dst + 0x98, 0);
    SetLong(dst + 0x9C, 0);
}

// original 0x494A80 (0x800A9894): one name window per living enemy at battle
// start. The living enemies' kinds (record +0x8C) are grouped into a list of
// up to eight {kind, count}, sorted by count, most first; the column's x and
// y come from 0x656A34 by the layout byte; then kind by kind, from the bottom
// up, each enemy of that kind (among the first 0x904AB2 records, alive or
// not, up to the kind's count) gets window record 0xC - n claimed
// (0x59E2D0(0xC - n, 3)) and filled: +2 = 4, +3 = 0, the x at +4 and +0x10,
// the row's y at +6 and +0x12, +0xA = the actor, +8 = +9 = 0. The window's
// handler draws the name (Battle_CopyEnemyData put it at record +0x80); this
// function reads no text.
//
// As the original has it:
//  - for layouts 0 and 1 the column is first measured (each kind's count * 8
//    + 0xD, and 2 between kinds) and starts that far below the table's y;
//  - the list is ended only by a kind of 0xFFFF: with eight different kinds
//    the walk runs on past the list's eight entries into the stack above the
//    original's frame - its return address, then the caller's frame - and
//    decrements a count there when a kind matches. Ours reads and writes
//    the same words: its entry stack pointer is the original's (the detour
//    is a jmp), so list entry 8 + k is the dword at entry esp + 4k.
extern "C" void __cdecl Battle_OpenEnemyNames(void) {
    std::uint16_t kinds[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    std::uint16_t counts[8] = {};
    unsigned char* const above = static_cast<unsigned char*>(__builtin_frame_address(0)) + 4;
    auto kind = [&](unsigned k) -> std::uint16_t { return k < 8 ? kinds[k] : Word(above + (k - 8) * 4); };
    auto count = [&](unsigned k) -> std::uint16_t { return k < 8 ? counts[k] : Word(above + (k - 8) * 4 + 2); };
    auto set_count = [&](unsigned k, unsigned v) {
        if (k < 8) counts[k] = static_cast<std::uint16_t>(v);
        else SetWord(above + (k - 8) * 4 + 2, v);
    };

    unsigned char listed = 0;
    unsigned char enemies = Battle_EnemyCount;
    if (enemies) {
        for (unsigned e = 0, left = enemies; left; --left, ++e) {
            auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy(e));
            if (!(enemy_rec->flags & 1)) continue;
            const std::uint16_t k = enemy_rec->enemy_kind;
            unsigned c = 0;
            while (c < listed && kinds[c] != k) ++c;
            if (c < listed) {
                ++counts[c];
                continue;
            }
            kinds[listed] = k;
            counts[listed] = 1;
            ++listed;
        }
        enemies = Battle_EnemyCount;
    }
    if (enemies > 1 && listed > 1)
        for (unsigned char d = 1; d < listed; ++d)
            for (unsigned j = static_cast<unsigned char>(d - 1); static_cast<int>(j) < listed - 1; ++j)
                if (static_cast<short>(counts[j]) < static_cast<short>(counts[j + 1])) {
                    const std::uint16_t tk = kinds[j], tc = counts[j];
                    kinds[j] = kinds[j + 1];
                    counts[j] = counts[j + 1];
                    kinds[j + 1] = tk;
                    counts[j + 1] = tc;
                }

    const unsigned layout = At(::bof3::Img(at::kLayout.v))[0];
    const std::uint16_t x = Word(At(::bof3::Img(at::kNamePlaces.v) + layout * 4));
    std::uint16_t y = Word(At(::bof3::Img(at::kNamePlaces.v) + layout * 4 + 2));
    if (layout < 2) {
        for (unsigned char k = 0;;) {
            y = static_cast<std::uint16_t>(y + static_cast<std::uint16_t>(count(k) << 3) + 0xD);
            ++k;
            if (kind(k) == 0xFFFF) break;
            y = static_cast<std::uint16_t>(y + 2);
        }
    }
    if (kind(0) == 0xFFFF) return;
    unsigned char shown = 0;
    unsigned char k = 0;
    do {
        unsigned char left = static_cast<unsigned char>(count(k));
        y = static_cast<std::uint16_t>(y - left * 8u - 0xD);
        const std::uint16_t top = y;
        std::uint16_t row = y;
        if (Battle_EnemyCount)
            for (unsigned char e = 0; left;) {
                if (kind(k) == rv::As<records::BattleEnemy>(Enemy(e))->enemy_kind) {
                    set_count(k, count(k) - 1u);
                    g.window_open(static_cast<unsigned char>(0xC - shown), 3);
                    unsigned char* const w = At(::bof3::Img(WindowRecords_addr) + 0x1B0 - shown * at::kWindowStride);
                    auto* const window_rec = rv::As<records::WindowRecord>(w);
                    window_rec->kind = 4;
                    window_rec->state = 0;
                    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->home_x, x);  // handler 3 (window_open's second argument): a battle window
                    window_rec->x = x;
                    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->home_y, row);
                    window_rec->y = row;
                    w[0xA] = static_cast<unsigned char>(e + 3);
                    w[8] = 0;
                    w[9] = 0;
                    ++shown;
                    --left;
                    row = static_cast<std::uint16_t>(row + 8);
                }
                if (!(++e < Battle_EnemyCount)) break;
            }
        y = static_cast<std::uint16_t>(top - 2);
        ++k;
    } while (kind(k) != 0xFFFF);
}

// original 0x494EA0: bit `kind` (16 bits) of the set at 0x904068 - an enemy
// kind the party has beaten before: Battle_EnemyDefeated (battle_flow.cpp)
// sets it, Battle_SetupEnemy copies it to the record's +0x8F, and the HP
// gauge (battle_windows.cpp EnemyGauge) and Identify's panel (magic_s12.cpp)
// read that copy. The set lies in the save's flag block (Cond_Flags +0xD8).
extern "C" unsigned char __cdecl Battle_EnemyKindFlag(unsigned kind) {
    const std::uint32_t k = kind & 0xFFFF;
    return (static_cast<std::uint32_t>(Long(At(::bof3::Img(Cond_Flags_addr) + 0xd8 + (k >> 5) * 4))) & (1u << (k & 31))) ? 1 : 0;
}

// original 0x494ED0: sets that bit. Called from 0x437470 (group BB).
extern "C" void __cdecl Battle_SetEnemyKindFlag(unsigned kind) {
    const std::uint32_t k = kind & 0xFFFF;
    OrLong(At(::bof3::Img(Cond_Flags_addr) + 0xd8 + (k >> 5) * 4), 1u << (k & 31));
}

// original 0x494F00: enemy `slot`'s +0xF2 / +0xF3 from the enemy data `id`
// (16 bits) by Sprite_Current's +8 (the layout ^ 2): 0 (+0x79, +0x7B),
// 1 (-+0x79, +0x7B), 2 (-+0x78, +0x7A), 3 (+0x78, +0x7A); others nothing.
extern "C" void __cdecl Battle_SetEnemyOffset(unsigned slot, unsigned id) {
    const unsigned char side = Sprite_Current[8];
    if (side > 3) return;
    const unsigned char* const data = EnemyData(id & 0xFFFF);
    unsigned char* const e = Enemy(slot & 0xFF);
    switch (side) {
    case 0: e[0xF2] = data[0x79]; e[0xF3] = data[0x7B]; break;
    case 1: e[0xF2] = static_cast<unsigned char>(-data[0x79]); e[0xF3] = data[0x7B]; break;
    case 2: e[0xF2] = static_cast<unsigned char>(-data[0x78]); e[0xF3] = data[0x7A]; break;
    default: e[0xF2] = data[0x78]; e[0xF3] = data[0x7A]; break;
    }
}

// original 0x588F00 (0x8014D154; PSX 0x8014D154): Sprite_Current +0x29 = Draw_OtSlot, then
// (a tail jump) Sprite_UpdateScreen.
extern "C" void __cdecl Sprite_UpdateScreenSlot(void) {
    Sprite_Current[0x29] = Draw_OtSlot;
    battle_sprites::g.update_screen();
}

// original 0x587900 (PSX 0x8015E210): Sound_PlayEffect(id). Ops BC, BD and D1 push more
// arguments; only the first is read.
//
// The PlayStation has three here (US 0x8015D71C / 0x8015D7C8 / 0x8015D820;
// JP 0x8015E10C / 0x8015E1B8 / 0x8015E210); the port folded them into one.
// Every caller but four is the first's (a jal scan of the US disc: 503 calls
// in 176 overlays; 0x8015D7C8 only from op BC and SCENA01's three, 0x8015D820
// only from ops BD and D1), and those four are ours and call sound_sp's
// level plays under DIV-0172 (move_groups.cpp, scena_sc1.cpp).
// DIVERGENCE DIV-0171: with the switch, the first's play: the cue follows
// Sprite_Current (sound_sp.h).
extern "C" void __cdecl Sound_PlayById(unsigned short id) {
    if (sound_sp::g_position) {
        sound_sp::PlayPositional(id);
        return;
    }
    battle_sprites::g.play_effect(id);
}

void BattleSprites_Inject() {
    if (bof3::WantsShadow("battle_sprites")) battle_sprites::SelfTest();
    BOF3_INJECT(Battle_InitEncounterKind);
    BOF3_INJECT(Battle_InitBossEncounter);
    BOF3_INJECT(Battle_InitEnemies);
    BOF3_INJECT(Battle_SetupEnemy);
    BOF3_INJECT(Battle_CopyEnemyData);
    BOF3_INJECT(Battle_OpenEnemyNames);
    BOF3_INJECT(Battle_EnemyKindFlag);
    BOF3_INJECT(Battle_SetEnemyKindFlag);
    BOF3_INJECT(Battle_SetEnemyOffset);
    BOF3_INJECT(Sprite_UpdateScreenSlot);
    BOF3_INJECT(Sound_PlayById);
    // DIV-0187 writes the enemy's +0x8F here; Capcom's Battle_SetupEnemy
    // would write the kind's bit and the cheat would be silently half off.
    if (Cheats_EnemyInfoShown() && !bof3::IsEnabled(bof3::addr::Battle_SetupEnemy))
        bof3::Fatal("BOF3X_ENEMY_INFO=1 needs Battle_SetupEnemy ours, and it is disabled");
}
