// Moved here by tools/move_funcs.py from battle_sprites.cpp, boss_spawn.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/battle/battle_4.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_sprites.h"
#include "game/battle_sprites_callees.h"
#include "game/boss_harness.h"
#include "game/boss_spawn.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/sound_sp.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3g::at;

}  // namespace

using namespace battle_sprites;

namespace battle_sprites {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

namespace {

unsigned char* Enemy(std::uint32_t i) { return At(::bof3::Img(Battle_Enemies_addr) + i * battle_sprites::at::kEnemyStride); }

unsigned char* EnemyData(std::uint32_t id) { return At(::bof3::Img(battle_sprites::at::kEnemyData.v) + id * battle_sprites::at::kEnemyDataStride); }

}  // namespace

}  // namespace battle_sprites

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::Word;

// kObjects: now Sprite_Objects (symbols.toml). Sprite_Objects
constexpr U kObjectStride = 0xA4;

constexpr unsigned kObjectCount = 30;

constexpr ::bof3::image::Va kEnemyData{0x8C55C8};    // the area's eight enemy data records

constexpr U kEnemyDataStride = 0x8C;

unsigned char* ObjectAt(unsigned i) { return At(::bof3::Img(Sprite_Objects_addr) + i * kObjectStride); }

bool Tagged(const unsigned char* o, unsigned tag) { return o[6] == 7 && o[0x9E] == (tag & 0xFF); }

// The actor BossActor_Find answers, or a Fatal where the original would write
// through its null answer.
unsigned char* FoundOrAbort(const char* who, unsigned tag) {
    unsigned char* const p = BH_CALL(BossActor_Find)(tag);
    if (p == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through the null pointer "
                    "BossActor_Find answers (docs/boss_h.md section 6)",
                    who, tag & 0xFF);
    return p;
}

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U ULRest3g(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3g(U a) { return ULRest3g(move_script::At(a)); }

void SetULRest3g(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetULRest3g(U a, U v) { SetULRest3g(At(a), v); }

}  // namespace

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
    const unsigned char layout = static_cast<unsigned char>(At(::bof3::Img(battle_sprites::at::kLayout.v))[0] ^ 2);
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

// original 0x494500 (called first by Battle_InitBossEncounter 0x4942A0): the
// encounter row EventBattle_Records[0x904AAA] + 2 names; for each of its eight
// slots whose kind is not 0xFF, Battle_PlaceBossActor(slot, the count so far,
// the kind); 0x904AB2 0 before, copied to 0x904AB3 after (the actors placed).
// The original passes the slot and the count from stack bytes whose upper
// three bytes it never wrote; its callee reads their low bytes only.
extern "C" void __cdecl Battle_PlaceBossActors(void) {
    const unsigned event = EventBattle_Id;
    const unsigned row = At(::bof3::Img(rest_3g::at::kEventRows.v) + 4 * event)[0];
    Battle_EnemyCount = 0;
    if (row >= rest_3g::at::kEncounterRows)
        bof3::Fatal("Battle_PlaceBossActors: event battle %u names encounter row %u, past Encounter_Rows' %u - the original "
                    "reads on into the enemy kinds' records (docs/rest_3g.md section 7)",
                    event, row, rest_3g::at::kEncounterRows);
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
    if (n >= rest_3g::at::kEnemyCount)
        bof3::Fatal("Battle_PlaceBossActor: count %u, past the %u enemy records - the original writes past them "
                    "(docs/rest_3g.md section 7)",
                    n, rest_3g::at::kEnemyCount);
    unsigned char* const e = At(::bof3::Img(Battle_Enemies_addr) + rest_3g::at::kEnemyStride * n);
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
    s[8] = static_cast<unsigned char>(At(::bof3::Img(rest_3g::at::kFormation.v))[0] ^ 2);
    s[0x48] = 0;
    SH_CALL(Battle_CopyEnemyData)(count, kind);
    psp_code::MyriaItem(At(::bof3::Img(Battle_Enemies_addr) + rest_3g::at::kEnemyStride * n), Game_AreaNumber, EventBattle_Id);
    SH_CALL(Battle_SetEnemyOffset)(count, kind & 0xFF);
    At(::bof3::Img(Battle_Enemies_addr) + rest_3g::at::kEnemyStride * n + 0x8F)[0] = Cheats_EnemyInfoShown() ? 1 : 0;   // DIV-0187
    actor[0] = static_cast<unsigned char>(actor[0] | 0x40);
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

// original 0x4948E0: the index 0..7 of the first of the loaded area's eight
// enemy data records (0x8C55C8 + i * 0x8C) whose byte +0xC is the tag's low
// byte, or 0xFF. Answers in al (the original keeps the caller's upper eax).
extern "C" unsigned char __cdecl EnemyData_FindByTag(unsigned tag) {
    for (unsigned i = 0; i < 8; ++i)
        if (At(::bof3::Img(kEnemyData.v) + i * kEnemyDataStride)[0xC] == (tag & 0xFF)) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x494920: the first field actor (Sprite_Objects 0..29, type +6 of
// 7) whose +0x9E is the tag's low byte, or null.
extern "C" unsigned char* __cdecl BossActor_Find(unsigned tag) {
    for (unsigned i = 0; i < kObjectCount; ++i)
        if (Tagged(ObjectAt(i), tag)) return ObjectAt(i);
    return nullptr;
}

// original 0x494980: BossActor_Find's search answering the index 0..29 in al,
// or 0xFF (the original keeps the caller's upper eax).
extern "C" unsigned char __cdecl BossActor_Index(unsigned tag) {
    for (unsigned i = 0; i < kObjectCount; ++i)
        if (Tagged(ObjectAt(i), tag)) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x4949D0: the tagged actor's +0 loses bit 0x40. BossActor_Find's
// answer is not tested (a Fatal here where the original faults).
extern "C" void __cdecl BossActor_ClearBit40(unsigned tag) {
    auto* const actor_rec = rv::As<records::SpriteObject>(FoundOrAbort("BossActor_ClearBit40", tag));
    actor_rec->flags &= 0xBF;
}

// original 0x4949F0: onto the tagged actor, by what's low byte - 0: from's
// pose (+0x4B the animation byte, the dwords +0x50 / +0x54, the words +0x58 /
// +0x5A) and +7 |= 0x20; 1: from's place (the dwords +0x34 / +0x38 / +0x3C);
// anything else: nothing, the actor looked for all the same. The answer is
// tested by neither copy (a Fatal here where the original faults); for any
// other `what` a missing actor is harmless in both.
extern "C" void __cdecl BossActor_CopyFrom(unsigned tag, const unsigned char* from, unsigned what) {
    unsigned char* const p = BH_CALL(BossActor_Find)(tag);
    auto* const actor_rec = rv::As<records::SpriteObject>(p);
    const unsigned mode = what & 0xFF;
    if (mode > 1) return;
    if (p == nullptr)
        bof3::Fatal("BossActor_CopyFrom: no field actor carries tag %u - the original writes through the null pointer "
                    "BossActor_Find answers (docs/boss_h.md section 6)",
                    tag & 0xFF);
    if (mode == 1) {
        actor_rec->x = Long(from + 0x34);
        actor_rec->z = Long(from + 0x38);
        actor_rec->height = Long(from + 0x3C);
        return;
    }
    actor_rec->animation = from[0x4B];
    actor_rec->script = Long(from + 0x50);
    actor_rec->frames = Long(from + 0x54);
    actor_rec->script_pos = Word(from + 0x58);
    actor_rec->frame_word = Word(from + 0x5A);
    actor_rec->flags2 |= 0x20;
}

// original 0x494A60: the tagged actor's bytes +0..+4 zeroed (a Fatal where
// the original faults on a missing actor).
extern "C" void __cdecl BossActor_Clear(unsigned tag) {
    unsigned char* const p = FoundOrAbort("BossActor_Clear", tag);
    for (unsigned i = 0; i < 5; ++i) p[i] = 0;
}

void Battle4_InjectFromBattleSprites() {
    BOF3_INJECT(Battle_InitEncounterKind);
    BOF3_INJECT(Battle_InitBossEncounter);
    BOF3_INJECT(Battle_InitEnemies);
    BOF3_INJECT(Battle_SetupEnemy);
    BOF3_INJECT(Battle_CopyEnemyData);
}

void Battle4_InjectFromBossSpawn() {
    BOF3_INJECT(EnemyData_FindByTag);
    BOF3_INJECT(BossActor_Find);
    BOF3_INJECT(BossActor_Index);
    BOF3_INJECT(BossActor_ClearBit40);
    BOF3_INJECT(BossActor_CopyFrom);
    BOF3_INJECT(BossActor_Clear);
}

void Battle4_InjectFromRest3g() {
    BOF3_INJECT(Battle_PlaceBossActors);
    BOF3_INJECT(Battle_PlaceBossActor);
}
